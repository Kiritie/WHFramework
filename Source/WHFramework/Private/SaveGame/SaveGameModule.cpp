#include "SaveGame/SaveGameModule.h"

#include "Event/EventModuleStatics.h"
#include "Event/Events/Common/Game/Event_GameExited.h"
#include "Gameplay/WHGameInstance.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "GameFramework/PlayerController.h"
#include "Main/MainModule.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "SaveGame/SaveDataSerializer.h"
#include "SaveGame/SaveGameStorage.h"
#include "SaveGame/SaveGameAsyncExecutor.h"
#include "Setting/SettingModule.h"
#include "Scene/SceneModule.h"
#include "Scene/Object/WorldTimer.h"
#include "Scene/Object/WorldWeather.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/Task/VoxelTerrainResources.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENTATION_MODULE(USaveGameModule)

namespace
{
struct FCreatedSlotCleanupTask
{
	TSharedPtr<const FSaveGenerationPlan, ESPMode::ThreadSafe> Plan;
	void operator()() const
	{
		FSaveGameStorage OwnerStorage(Plan->UserIndex, Plan->StorageRoot, Plan->TransactionId);
		if (OwnerStorage.BeginWorldWrite(Plan->SaveId, nullptr, {}) != ESaveWorldWriteResult::Acquired)
		{
			UE_LOG(LogTemp, Warning, TEXT("Created slot cleanup could not acquire native write ownership; directory retained: %s"), *Plan->SaveId.ToString());
			return;
		}
		FSaveManifest Current;
		// 只删除原请求创建、且仍没有已提交代的槽；任务不读取模块或 World。
		if (OwnerStorage.ReadManifest(Plan->SaveId, Current) && Current.CurrentGeneration == 0 &&
			!OwnerStorage.DeleteWorldDirectory(Plan->SaveId))
			UE_LOG(LogTemp, Error, TEXT("Failed save capture left an uncommitted created slot: %s"), *Plan->SaveId.ToString());
	}
};

void RestoreSceneEnvironment(UModuleBase* Module, const FParameter& Data, EPhase Phase)
{
	USceneModule* Scene = Cast<USceneModule>(Module);
	const FSceneModuleSaveData* SceneData = Data.GetPtr<FSceneModuleSaveData>();
	if(!Scene || !SceneData) return;

	// Keep timer/weather restoration explicit at the save transaction boundary.
	// This also repairs projects that temporarily shipped SceneModule with these
	// two agent-load calls commented out. Reapplying the same values is safe.
	if(UWorldTimer* Timer = Scene->GetWorldTimer())
	{
		if(Timer->IsAutoSave()) Timer->LoadSaveData(FParameter(SceneData->TimerData), Phase);
		Timer->OnRefresh(0.f);
	}
	if(UWorldWeather* Weather = Scene->GetWorldWeather())
	{
		if(Weather->IsAutoSave()) Weather->LoadSaveData(FParameter(SceneData->WeatherData), Phase);
		Weather->OnRefresh(0.f);
	}
}
}

USaveGameModule::USaveGameModule()
{
	ModuleName = FName("SaveGameModule");
	ModuleDisplayName = FText::FromString(TEXT("Save Game Module"));
	bModuleRequired = true;
	SaveScope = ESaveScope::None;
	UserIndex = 0;
	bSaveOperationRunning = false;
	bHasPendingSaveSlot = false;
	PendingSaveSlotParams = FCreateSaveSlotParams();
}

USaveGameModule::~USaveGameModule()
{
	TERMINATION_MODULE(USaveGameModule)
}

#if WITH_EDITOR
void USaveGameModule::OnDestroy()
{
	Super::OnDestroy();
	TERMINATION_MODULE(USaveGameModule)
}
#endif

void USaveGameModule::RestoreLastActiveSave()
{
	ActiveSaveId.Invalidate();
	if(!Storage)
	{
		return;
	}

	if(Storage->ReadLastActiveSave(ActiveSaveId))
	{
		return;
	}

	// Migrate installs created before last_active.txt existed. Only complete,
	// committed saves are exposed by GetSaveSlotSummaries().
	Storage->ClearLastActiveSave();
	const TArray<FSaveSlotSummary> Saves = GetSaveSlotSummaries();
	if(!Saves.IsEmpty())
	{
		ActiveSaveId = Saves[0].SaveId;
		if(!Storage->WriteLastActiveSave(ActiveSaveId))
		{
			UE_LOG(LogTemp, Warning, TEXT("Could not persist the recovered last active save."));
		}
	}
}

void USaveGameModule::OnInitialize()
{
	Super::OnInitialize();
	Storage = MakeUnique<FSaveGameStorage>(UserIndex);
	Storage->EnsureRoot();
	Storage->CleanupAllTempGenerations();
	RestoreLastActiveSave();
	if (UWHGameInstance* GameInstance = GetWorld() ? Cast<UWHGameInstance>(GetWorld()->GetGameInstance()) : nullptr)
	{
		PendingLoadContext = GameInstance->ConsumePendingSaveLoad();
	}
	UEventModuleStatics::SubscribeEvent<FEventGameExited>(this, &ThisClass::OnGameExited);
}

void USaveGameModule::OnPreparatory(EPhase InPhase)
{
	Super::OnPreparatory(InPhase);
	if (PHASEC(InPhase, EPhase::Final) && PendingLoadContext.IsValid())
	{
		const FPendingSaveLoadContext Context = PendingLoadContext;
		PendingLoadContext = FPendingSaveLoadContext();
		RestoreSlotGeneration(Context, Context.LoadPhase);
	}
}

void USaveGameModule::OnTermination(EPhase InPhase)
{
	if (bApplyingSaveCallbacks || bSaveTerminationRunning)
	{
		DeferredSaveTermination = EPhase(uint8(DeferredSaveTermination) | uint8(InPhase));
		if (AsyncSave) AsyncSave->RequestCancel();
		return;
	}
	{
		TGuardValue<bool> Terminating(bSaveTerminationRunning, true);
		bPendingUserQuit = false;
		PollPendingSave();
		if (bSaveOperationRunning)
		{
			bPendingUserQuit = false;
			if (!AsyncCompletionResult.IsSet())
			{
				if (AsyncSave) AsyncSave->RequestCancel();
				CompleteAsyncSave(FSaveOperationResult::Failed(ESaveResultCode::Busy,
					FText::FromString(TEXT("World termination interrupted the save; native task retirement remains owned until its actual exit"))));
			}
			// 明确销毁按原事务收尾；正常 Tick 只推进一个有预算的步骤。
			while (AsyncCompletionResult.IsSet()) AdvanceAsyncSaveCompletion(nullptr, true);
		}
		Super::OnTermination(InPhase);
		if (PHASEC(InPhase, EPhase::Final))
		{
			AsyncSave.Reset();
			AsyncExecutorLease.Reset();
			Storage.Reset();
		}
		DeferredSaveTermination = EPhase(uint8(DeferredSaveTermination) & ~uint8(InPhase));
	}
	FlushDeferredSaveTermination();
}

void USaveGameModule::OnRefresh(float DeltaSeconds, bool bInEditor)
{
	Super::OnRefresh(DeltaSeconds, bInEditor);
	if (AsyncCreatedSlotCleanup.IsCompleted()) AsyncCreatedSlotCleanup = {};
	if (bInEditor || !bSaveOperationRunning)
	{
		return;
	}
	const bool bCaptureAdvanced = AsyncPlan.IsValid() && !AsyncCompletionResult.IsSet();
	if (bCaptureAdvanced) AdvanceAsyncSaveCapture();
	PollPendingSave();
	if (!bCaptureAdvanced && AsyncCompletionResult.IsSet() && !AsyncVoxelOwner) AdvanceAsyncSaveCompletion();
}

void USaveGameModule::LoadData(const FParameter& InSaveData, EPhase InPhase)
{
}

FParameter USaveGameModule::ToData()
{
	return FParameter();
}

void USaveGameModule::SetUserIndex(int32 InUserIndex)
{
	if (IsSaveOperationRunning())
	{
		return;
	}
	DetachActiveSave();
	UserIndex = InUserIndex;
	Storage = MakeUnique<FSaveGameStorage>(UserIndex);
	Storage->EnsureRoot();
	Storage->CleanupAllTempGenerations();
	RestoreLastActiveSave();
}

FSaveOperationResult USaveGameModule::CreateSaveSlot(const FCreateSaveSlotParams& Params, FSaveSlotSummary& OutSummary)
{
	if (!Storage || IsSaveOperationRunning() || !GetWorld() || GetWorld()->GetNetMode() == NM_Client)
	{
		return FSaveOperationResult::Failed(ESaveResultCode::Busy, FText::FromString(TEXT("Cannot create world slot now")));
	}

	const FGuid PreviousActiveSaveId = ActiveSaveId;
	FSaveManifest Manifest;
	Manifest.SaveId = FGuid::NewGuid();
	Manifest.DisplayName = Params.DisplayName;
	Manifest.Description = Params.Description;
	Manifest.CreatedAt = FDateTime::UtcNow();
	Manifest.UpdatedAt = Manifest.CreatedAt;
	Manifest.CurrentMap = Params.InitialMap;

	{
		TGuardValue<bool> Guard(bSaveOperationRunning, true);
		if (Storage->BeginWorldWrite(Manifest.SaveId, nullptr, {}) != ESaveWorldWriteResult::Acquired)
			return FSaveOperationResult::Failed(ESaveResultCode::Busy, FText::FromString(TEXT("New save slot is owned by another native writer")));
		ON_SCOPE_EXIT { Storage->EndWorldWrite(); };
		if (!Storage->CreateWorldDirectory(Manifest.SaveId) || !Storage->WriteManifestAtomic(Manifest.SaveId, Manifest))
		{
			Storage->DeleteWorldDirectory(Manifest.SaveId);
			return FSaveOperationResult::Failed(ESaveResultCode::WriteFailed, FText::FromString(TEXT("Create save slot failed")));
		}
	}

	if (Params.bCaptureCurrentWorld)
	{
		PendingCreatedSaveId = Manifest.SaveId;
		PendingCreationPreviousSaveId = PreviousActiveSaveId;
		const FSaveOperationResult Result = SaveSlotAsync(Manifest.SaveId);
		if (!Result)
		{
			Storage->DeleteWorldDirectory(Manifest.SaveId);
			PendingCreatedSaveId.Invalidate();
			ActiveSaveId = PreviousActiveSaveId;
			return Result;
		}
		OutSummary = Manifest.ToSummary(Storage->GetWorldDir(Manifest.SaveId));
		OutSummary.bValid = false;
		return Result;
	}
	else
	{
		ActiveSaveId = Manifest.SaveId;
	}

	OutSummary = Manifest.ToSummary(Storage->GetWorldDir(Manifest.SaveId));
	return FSaveOperationResult::Success();
}

FSaveOperationResult USaveGameModule::SaveSlot(FGuid SaveId)
{
	// 同一请求入口返回 Accepted；只有实际完成回执才返回 Success。
	return SaveSlotAsync(SaveId);
}

FSaveOperationResult USaveGameModule::SaveActiveSlot()
{
	return ActiveSaveId.IsValid() ? SaveSlot(ActiveSaveId) : FSaveOperationResult::Failed(ESaveResultCode::NotFound, FText::FromString(TEXT("No active save")));
}

FSaveOperationResult USaveGameModule::SaveSlotAsync(FGuid SaveId)
{
	check(IsInGameThread());
	if (!Storage || IsSaveOperationRunning() || bApplyingSaveCallbacks || bSaveTerminationRunning ||
		!SaveId.IsValid() || !GetWorld() || GetWorld()->GetNetMode() == NM_Client)
	{
		return FSaveOperationResult::Failed(ESaveResultCode::Busy, FText::FromString(TEXT("World save is unavailable or busy")));
	}
	AMainModule* Main = AMainModule::GetPtr(GetWorld());
	if (!Main)
		return FSaveOperationResult::Failed(ESaveResultCode::ModuleMissing, FText::FromString(TEXT("MainModule missing.")));
	const int32 MaximumModules = Main->GetModules().Num();
	UVoxelModule* VoxelOwner = UVoxelModule::GetPtr(GetWorld());
	if (VoxelOwner && (!VoxelOwner->IsSaveEnabled() || VoxelOwner->GetSaveScope() != ESaveScope::World)) VoxelOwner = nullptr;
	if (VoxelOwner && !VoxelOwner->IsReady())
		return FSaveOperationResult::Failed(ESaveResultCode::CaptureFailed,
			FText::FromString(TEXT("Create or load a ready voxel world before accepting its save capture")));
	const FGuid OperationId = FGuid::NewGuid();
	const auto Resources = VoxelOwner ? VoxelOwner->GetTerrainResources() : nullptr;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> MetadataLease;
	if (Resources)
	{
		const uint64 Bytes = VoxelBoundedAllocation::WorkerSharedBytes<FSaveGenerationPlan>() +
			VoxelBoundedAllocation::TaskBytes<FCreatedSlotCleanupTask>() +
			VoxelBoundedAllocation::ArrayBytes<TObjectPtr<UModuleBase>>(MaximumModules) +
			VoxelBoundedAllocation::ArrayBytes<FSaveModuleBytes>(MaximumModules) +
			VoxelBoundedAllocation::ArrayBytes<TCHAR>(Storage->GetRootDir().Len() + 1);
		if (!Resources->Acquire(FVoxelGenerationConsumerKey::Section(VoxelOwner->GetWorldEpoch(), OperationId,
			1, {}, 8), EVoxelTerrainResourcePool::Input, Bytes, MetadataLease))
			return FSaveOperationResult::Failed(ESaveResultCode::Busy,
				FText::FromString(TEXT("CapacityBlocked: save plan/module owner backing was not granted")));
	}
	AsyncCaptureMetadataLease = MetadataLease;
	AsyncPlan = Resources ? VoxelBoundedAllocation::MakeWorkerShared<FSaveGenerationPlan>(MetadataLease) :
		MakeShared<FSaveGenerationPlan, ESPMode::ThreadSafe>();
	FSaveGenerationPlan& Plan = *AsyncPlan;
	Plan.MetadataLease = MetadataLease;
	Plan.SaveId = SaveId;
	Plan.TransactionId = OperationId;
	Plan.UserIndex = UserIndex;
	Plan.StorageRoot = Storage->GetRootDir();
	Plan.Manifest.SaveId = SaveId;
	Plan.bResolveManifestOnWorker = true;
	Plan.bWriteLastActiveSave = true;
	Plan.CurrentMap = FName(*UGameplayStatics::GetCurrentLevelName(GetWorld(), true));
	Plan.bDeleteCreatedSlotOnFailure = PendingCreatedSaveId == SaveId;
	bSaveOperationRunning = true;
	bAsyncWorkerStarted = false;
	bAsyncWorkerCapacityBlocked = false;
	AsyncOperationId = OperationId;
	AsyncSaveId = SaveId;
	AsyncWorld = GetWorld();
	AsyncVoxelOwner = VoxelOwner;
	AsyncCaptured.Reserve(MaximumModules);
	Plan.Modules.Reserve(MaximumModules);
	for (UModuleBase* Module : Main->GetModules())
		if (Module && Module->GetSaveScope() == ESaveScope::World && Module->IsSaveEnabled()) AsyncCaptured.Add(Module);
	AsyncCaptured.Sort([](const UModuleBase& A, const UModuleBase& B)
	{
		return A.GetModuleIndex() < B.GetModuleIndex();
	});
	AsyncPrepareCount = 0;
	AsyncBuildCursor = 0;
	AsyncCompletionResult.Reset();
	AsyncCompletionCursor = -1;
	bAsyncCompletionFilesCommitted = false;
	return FSaveOperationResult::Accepted(AsyncOperationId);
}

void USaveGameModule::AdvanceWorldSaveCapture(UVoxelModule& InVoxel)
{
	check(IsInGameThread());
	if (AsyncWorld != InVoxel.GetWorld() || AsyncVoxelOwner != &InVoxel) return;
	PollNativeSave(&InVoxel.GetTerrainFrameContext());
	if (AsyncCompletionResult.IsSet()) AdvanceAsyncSaveCompletion(&InVoxel.GetTerrainFrameContext());
	else AdvanceAsyncSaveCapture(&InVoxel.GetTerrainFrameContext());
}

bool USaveGameModule::HasWorldSaveWork(const UVoxelModule& InVoxel) const
{
	check(IsInGameThread());
	return bSaveOperationRunning && AsyncWorld == InVoxel.GetWorld() && AsyncVoxelOwner == &InVoxel;
}

void USaveGameModule::AdvanceAsyncSaveCapture(FVoxelTerrainFrameContext* InFrame)
{
	check(IsInGameThread());
	if (!AsyncPlan || !bSaveOperationRunning || bApplyingSaveCallbacks || AsyncCompletionResult.IsSet()) return;
	if (!::IsValid(AsyncWorld.Get()) || GetWorld() != AsyncWorld)
	{
		CompleteAsyncSave(FSaveOperationResult::Failed(ESaveResultCode::CaptureFailed,
			FText::FromString(TEXT("Save capture lost its original world owner"))));
		return;
	}
	UVoxelModule* VoxelOwner = AsyncVoxelOwner;
	const bool bPreparationPending = AsyncPrepareCount < AsyncCaptured.Num();
	if (VoxelOwner)
	{
		// Voxel 捕获和模块序列化共用它当前帧的预算；普通 SaveGame Tick 不重开预算。
		if (!::IsValid(VoxelOwner) || VoxelOwner->GetWorld() != AsyncWorld)
		{
			CompleteAsyncSave(FSaveOperationResult::Failed(ESaveResultCode::CaptureFailed,
				FText::FromString(TEXT("Save capture lost its original voxel module owner"))));
			return;
		}
		if (!VoxelOwner->IsReady())
		{
			CompleteAsyncSave(FSaveOperationResult::Failed(ESaveResultCode::CaptureFailed,
				FText::FromString(TEXT("Original voxel world stopped serving its accepted save capture"))));
			return;
		}
		if (!InFrame || !InFrame->IsActive()) return;
		if (!bPreparationPending)
		{
			FString Error;
			const auto State = VoxelOwner->GetSaveCaptureState(Error);
			if (State == EVoxelSceneCaptureStep::Failed)
			{
				CompleteAsyncSave(FSaveOperationResult::Failed(ESaveResultCode::CaptureFailed, FText::FromString(Error)));
				return;
			}
			if (State != EVoxelSceneCaptureStep::Complete) return;
		}
	}
	if (bAsyncWorkerCapacityBlocked && AsyncPlan->Voxel.IsSet())
	{
		const auto Resources = AsyncPlan->Voxel->Voxels.Journal.Resources;
		if (Resources && Resources->Snapshot().ReleaseVersion <= AsyncWorkerBlockedReleaseVersion) return;
		bAsyncWorkerCapacityBlocked = false;
	}
	const double Started = FPlatformTime::Seconds();
	if (InFrame && !InFrame->GetPublishBudget().TryStart(EVoxelPublishPhase::Prepare, Started, AsyncCaptureCostMilliseconds))
	{
		if (!InFrame->IsOverdueOpportunityAvailable()) return;
		InFrame->ConsumeOverdueOpportunity();
	}
	FSaveOperationResult Failure = FSaveOperationResult::Success();
	FString Error;
	bool bCapacityBlocked = false;
	if (bPreparationPending)
	{
		UModuleBase* Module = AsyncCaptured[AsyncPrepareCount];
		if (!::IsValid(Module) || Module->GetWorld() != AsyncWorld)
			Failure = FSaveOperationResult::Failed(ESaveResultCode::CaptureFailed,
				FText::FromString(TEXT("Save preparation lost its original module owner")));
		else
		{
			TGuardValue<bool> Applying(bApplyingSaveCallbacks, true);
			++AsyncPrepareCount;
			Module->OnBeforeSaveData();
		}
	}
	if (!bPreparationPending && VoxelOwner && !AsyncPlan->Voxel.IsSet())
	{
		FVoxelModuleSaveCapture Capture;
		if (!VoxelOwner->CopySaveCapture(Capture, Error))
			Failure = FSaveOperationResult::Failed(ESaveResultCode::CaptureFailed, FText::FromString(Error));
		else
		{
			AsyncPlan->TransactionId = Capture.Voxels.TransactionId;
			AsyncPlan->Voxel = MoveTemp(Capture);
		}
	}
	if (!bPreparationPending && Failure && VoxelOwner && AsyncPlan->Voxel.IsSet() &&
		!AsyncPlan->Voxel->Voxels.Completion->ExpectedStorageRoot &&
		!VoxelOwner->BindSaveCommitOwner(AsyncSaveId, AsyncPlan->StorageRoot, Error))
	{
		bCapacityBlocked = Error.StartsWith(TEXT("CapacityBlocked:"));
		if (!bCapacityBlocked) Failure = FSaveOperationResult::Failed(ESaveResultCode::CaptureFailed, FText::FromString(Error));
	}
	if (!bPreparationPending && Failure && !bCapacityBlocked && AsyncBuildCursor < AsyncCaptured.Num())
	{
		TGuardValue<bool> Applying(bApplyingSaveCallbacks, true);
		if (!AdvanceModuleFileCapture(Error, bCapacityBlocked) && !bCapacityBlocked)
			Failure = FSaveOperationResult::Failed(ESaveResultCode::SerializeFailed, FText::FromString(Error));
	}
	else if (!bPreparationPending && Failure && !bCapacityBlocked)
	{
		if (!AsyncSave)
		{
			if (AsyncPlan->Voxel.IsSet())
			{
				const auto& Capture = AsyncPlan->Voxel->Voxels;
				bCapacityBlocked = !Capture.Journal.Resources->Acquire(FVoxelGenerationConsumerKey::Section(
					Capture.WorldEpoch, Capture.TransactionId, 1, {}, 10), EVoxelTerrainResourcePool::Input,
					FMemory::QuantizeSize(sizeof(FSaveGameAsyncExecutor), alignof(FSaveGameAsyncExecutor)), AsyncExecutorLease);
			}
			if (!bCapacityBlocked) AsyncSave = MakeUnique<FSaveGameAsyncExecutor>();
		}
		if (!bCapacityBlocked && !AsyncSave->Start(MoveTemp(*AsyncPlan), &Error))
		{
			if (Error.StartsWith(TEXT("CapacityBlocked:")) && AsyncPlan->Voxel.IsSet())
			{
				bCapacityBlocked = true;
			}
			else Failure = FSaveOperationResult::Failed(ESaveResultCode::Busy, FText::FromString(Error));
		}
		else if (!bCapacityBlocked)
		{
			bAsyncWorkerStarted = true;
			AsyncPlan.Reset();
		}
	}
	if (bCapacityBlocked && AsyncPlan && AsyncPlan->Voxel.IsSet())
	{
		bAsyncWorkerCapacityBlocked = true;
		AsyncWorkerBlockedReleaseVersion = AsyncPlan->Voxel->Voxels.Journal.Resources->Snapshot().ReleaseVersion;
	}
	const double Finished = FPlatformTime::Seconds();
	const double Actual = (Finished - Started) * 1000.0;
	AsyncCaptureCostMilliseconds = FMath::Lerp(AsyncCaptureCostMilliseconds, Actual, 0.15);
	if (InFrame)
	{
		InFrame->GetPublishBudget().Finish(EVoxelPublishPhase::Prepare, Started, Finished);
		if (VoxelOwner && Actual > VoxelOwner->GetMaximumSceneCaptureStepMilliseconds() && AsyncPlan)
		{
			Failure = FSaveOperationResult::Failed(ESaveResultCode::CaptureFailed, FText::FromString(FString::Printf(
				TEXT("Save module capture step capability error: %.3fms exceeds %.3fms"), Actual, VoxelOwner->GetMaximumSceneCaptureStepMilliseconds())));
		}
	}
	if (!Failure) CompleteAsyncSave(Failure);
	FlushDeferredSaveTermination();
}

bool USaveGameModule::AdvanceModuleFileCapture(FString& OutError, bool& bOutCapacityBlocked)
{
	check(IsInGameThread() && AsyncPlan && AsyncBuildCursor < AsyncCaptured.Num());
	bOutCapacityBlocked = false;
	OutError.Reset();
	UModuleBase* Module = AsyncCaptured[AsyncBuildCursor];
	if (!::IsValid(Module) || Module->GetWorld() != AsyncWorld)
	{
		OutError = TEXT("Module file capture lost its original world/module owner");
		return false;
	}
	const auto Resources = AsyncPlan->Voxel.IsSet() ? AsyncPlan->Voxel->Voxels.Journal.Resources : nullptr;
	const auto Consumer = AsyncPlan->Voxel.IsSet() ? FVoxelGenerationConsumerKey::Section(
		AsyncPlan->Voxel->Voxels.WorldEpoch, AsyncPlan->Voxel->Voxels.TransactionId,
		uint64(AsyncBuildCursor) + 1, {}, 9) : FVoxelGenerationConsumerKey();
	auto Reserve = [&](TArray<uint8>& Buffer, const int32 Required,
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& Lease, const EVoxelTerrainResourcePool Pool)
	{
		if (Required <= Buffer.Max()) return true;
		const int32 Capacity = int32(FMath::Min<int64>(MAX_int32,
			FMath::Max<int64>(Required, FMath::Max<int64>(4096, int64(Buffer.Max()) * 2))));
		const uint64 Peak = Buffer.GetAllocatedSize() + VoxelBoundedAllocation::ArrayBytes<uint8>(Capacity);
		if (Resources)
		{
			const auto Limits = Resources->Snapshot().Limits;
			const uint64 Limit = Pool == EVoxelTerrainResourcePool::Working ? Limits.WorkingBytes : Limits.InputBytes;
			if (Peak > Limit)
			{
				OutError = TEXT("Module serialization capability error: backing growth exceeds the configured pool capacity");
				return false;
			}
		}
		if (Resources && !(Lease ? Lease->Resize(Peak) : Resources->Acquire(Consumer, Pool, Peak, Lease)))
		{
			bOutCapacityBlocked = true;
			OutError = TEXT("CapacityBlocked: original module payload/file backing is retained for retry");
			return false;
		}
		Buffer.Reserve(Capacity);
		if (Lease) verify(Lease->Resize(Buffer.GetAllocatedSize()));
		return true;
	};
	constexpr int32 WindowBytes = 64 * 1024;
	switch (AsyncModuleCapturePhase)
	{
	case 0:
		AsyncModuleFile.Emplace();
		AsyncModuleFile->ModuleName = Module->GetModuleName();
		AsyncModuleFile->Version = Module->GetSaveDataVersion();
		AsyncModuleData = Module->GetSaveData(true);
		if (DeferredSaveTermination != EPhase::None)
		{
			OutError = TEXT("Module capture callback requested world termination");
			return false;
		}
		if (!AsyncModuleData.HasValue())
		{
			OutError = FString::Printf(TEXT("Module %s returned no captured save payload"), *Module->GetModuleName().ToString());
			return false;
		}
		AsyncModuleCapturePhase = 1;
		break;
	case 1:
		if (!FSaveDataSerializer::SerializeParameterBounded(AsyncModuleData, MAX_int32, AsyncModulePayload,
			[&](TArray<uint8>& Buffer, const int32 Required)
			{
				return Reserve(Buffer, Required, AsyncModulePayloadLease, EVoxelTerrainResourcePool::Working);
			}))
		{
			if (OutError.IsEmpty()) OutError = TEXT("Captured module reflection serialization failed");
			return false;
		}
		AsyncModulePayloadCursor = 0;
		AsyncModulePayloadCrc = 0;
		AsyncModuleCapturePhase = 2;
		break;
	case 2:
	{
		const int32 Count = FMath::Min(WindowBytes, AsyncModulePayload.Num() - AsyncModulePayloadCursor);
		if (Count > 0)
		{
			AsyncModulePayloadCrc = FCrc::MemCrc32(AsyncModulePayload.GetData() + AsyncModulePayloadCursor, Count, AsyncModulePayloadCrc);
			AsyncModulePayloadCursor += Count;
		}
		if (AsyncModulePayloadCursor == AsyncModulePayload.Num()) AsyncModuleCapturePhase = 3;
		break;
	}
	case 3:
		if (!FSaveDataSerializer::WriteModuleHeaderBounded(AsyncModuleFile->ModuleName, AsyncModuleFile->Version,
			AsyncModulePayload.Num(), AsyncModulePayloadCrc, AsyncModuleFile->Bytes,
			[&](TArray<uint8>& Buffer, const int32 Required)
			{
				return Reserve(Buffer, Required, AsyncModuleFile->ResourceLease, EVoxelTerrainResourcePool::Input);
			}))
		{
			if (OutError.IsEmpty()) OutError = TEXT("Captured module header serialization failed");
			return false;
		}
		if (AsyncModulePayload.Num() > MAX_int32 - AsyncModuleFile->Bytes.Num())
		{
			OutError = TEXT("Captured module file exceeds the byte-array protocol capacity");
			return false;
		}
		// 在正文复制前一次授予最终大小，避免后续每个窗口重新搬动旧正文。
		if (!Reserve(AsyncModuleFile->Bytes, AsyncModuleFile->Bytes.Num() + AsyncModulePayload.Num(),
			AsyncModuleFile->ResourceLease, EVoxelTerrainResourcePool::Input)) return false;
		AsyncModulePayloadCursor = 0;
		AsyncModuleCapturePhase = 4;
		break;
	case 4:
	{
		const int32 Count = FMath::Min(WindowBytes, AsyncModulePayload.Num() - AsyncModulePayloadCursor);
		if (Count > 0)
		{
			AsyncModuleFile->Bytes.Append(AsyncModulePayload.GetData() + AsyncModulePayloadCursor, Count);
			AsyncModulePayloadCursor += Count;
		}
		if (AsyncModulePayloadCursor == AsyncModulePayload.Num()) AsyncModuleCapturePhase = 5;
		break;
	}
	case 5:
		AsyncPlan->Modules.Add(MoveTemp(AsyncModuleFile.GetValue()));
		ResetModuleFileCapture();
		++AsyncBuildCursor;
		break;
	default:
		OutError = TEXT("Invalid persistent module capture phase");
		return false;
	}
	return true;
}

void USaveGameModule::ResetModuleFileCapture()
{
	AsyncModuleData.Reset();
	AsyncModulePayload.Empty();
	AsyncModulePayloadLease.Reset();
	AsyncModuleFile.Reset();
	AsyncModuleCapturePhase = 0;
	AsyncModulePayloadCursor = 0;
	AsyncModulePayloadCrc = 0;
}

void USaveGameModule::FlushDeferredSaveTermination()
{
	if (bApplyingSaveCallbacks || bSaveTerminationRunning || DeferredSaveTermination == EPhase::None) return;
	const EPhase Phase = DeferredSaveTermination;
	DeferredSaveTermination = EPhase::None;
	bPendingUserQuit = false;
	OnTermination(Phase);
}

FSaveOperationResult USaveGameModule::SaveActiveSlotAsync()
{
	return ActiveSaveId.IsValid() ? SaveSlotAsync(ActiveSaveId)
	                              : FSaveOperationResult::Failed(ESaveResultCode::NotFound, FText::FromString(TEXT("No active save")));
}

void USaveGameModule::CompleteAsyncSave(const FSaveOperationResult& Result)
{
	check(IsInGameThread());
	if (!bSaveOperationRunning || AsyncCompletionResult.IsSet()) return;
	AsyncCompletionResult = Result;
	AsyncCompletionResult->OperationId = AsyncOperationId;
	bAsyncCompletionFilesCommitted = bool(Result);
	AsyncCompletionCursor = -1;
}

void USaveGameModule::AdvanceAsyncSaveCompletion(FVoxelTerrainFrameContext* InFrame, const bool bForce)
{
	check(IsInGameThread());
	if (!AsyncCompletionResult.IsSet() || bApplyingSaveCallbacks) return;
	UVoxelModule* const VoxelOwner = AsyncVoxelOwner;
	if (!bForce && VoxelOwner && (!InFrame || !InFrame->IsActive())) return;
	const double Started = FPlatformTime::Seconds();
	if (!bForce && InFrame &&
		!InFrame->GetPublishBudget().TryStart(EVoxelPublishPhase::Retire, Started, AsyncCompletionCostMilliseconds))
	{
		if (!InFrame->IsOverdueOpportunityAvailable()) return;
		InFrame->ConsumeOverdueOpportunity();
	}
	if (AsyncCompletionCursor < 0)
	{
		if (PendingCreatedSaveId == AsyncSaveId && !bAsyncCompletionFilesCommitted && !bAsyncWorkerStarted && AsyncPlan)
		{
			check(AsyncCreatedSlotCleanup.IsCompleted());
			AsyncCreatedSlotCleanup = UE::Tasks::Launch(UE_SOURCE_LOCATION, FCreatedSlotCleanupTask{AsyncPlan});
		}
		AsyncPlan.Reset();
		ResetModuleFileCapture();
		AsyncCompletionCursor = 0;
	}
	else if (AsyncCompletionCursor < AsyncPrepareCount)
	{
		UModuleBase* Module = AsyncCaptured[AsyncCompletionCursor++];
		TGuardValue<bool> Applying(bApplyingSaveCallbacks, true);
		if (::IsValid(Module))
		{
			Module->OnAfterSaveData(bAsyncCompletionFilesCommitted);
			if (bAsyncCompletionFilesCommitted && Module == AsyncVoxelOwner && !AsyncVoxelOwner->DidApplySaveCommit())
			{
				AsyncCompletionResult->bSuccess = false;
				AsyncCompletionResult->Code = ESaveResultCode::CommitFailed;
				AsyncCompletionResult->Message = FText::FromString(TEXT("Save files committed, but the voxel runtime did not accept its completion receipt"));
			}
		}
	}
	else FinalizeAsyncSave();
	const double Finished = FPlatformTime::Seconds();
	const double Actual = (Finished - Started) * 1000.0;
	AsyncCompletionCostMilliseconds = FMath::Lerp(AsyncCompletionCostMilliseconds, Actual, 0.15);
	if (!bForce && InFrame)
	{
		InFrame->GetPublishBudget().Finish(EVoxelPublishPhase::Retire, Started, Finished);
		if (VoxelOwner && Actual > VoxelOwner->GetMaximumSceneCaptureStepMilliseconds())
		{
			const FString Error = FString::Printf(TEXT("Save completion capability error: %.3fms exceeds %.3fms"),
				Actual, VoxelOwner->GetMaximumSceneCaptureStepMilliseconds());
			if (AsyncCompletionResult.IsSet())
			{
				AsyncCompletionResult->bSuccess = false;
				AsyncCompletionResult->Code = ESaveResultCode::CommitFailed;
				AsyncCompletionResult->Message = FText::FromString(Error);
			}
			else UE_LOG(LogTemp, Error, TEXT("%s"), *Error);
		}
	}
	FlushDeferredSaveTermination();
}

void USaveGameModule::FinalizeAsyncSave()
{
	check(IsInGameThread() && AsyncCompletionResult.IsSet() && AsyncCompletionCursor == AsyncPrepareCount);
	const FGuid CompletedId = AsyncSaveId;
	const FSaveOperationResult Completed = AsyncCompletionResult.GetValue();
	const bool bFilesCommitted = bAsyncCompletionFilesCommitted;
	AsyncSaveId.Invalidate();
	AsyncOperationId.Invalidate();
	AsyncCompletionResult.Reset();
	AsyncCompletionCursor = -1;
	bAsyncCompletionFilesCommitted = false;
	AsyncPrepareCount = 0;
	AsyncCaptured.Empty();
	AsyncVoxelOwner = nullptr;
	AsyncCaptureMetadataLease.Reset();
	if (bFilesCommitted)
	{
		ActiveSaveId = CompletedId;
	}
	if (PendingCreatedSaveId == CompletedId)
	{
		if (!bFilesCommitted)
		{
			ActiveSaveId = PendingCreationPreviousSaveId;
		}
		PendingCreatedSaveId.Invalidate();
		PendingCreationPreviousSaveId.Invalidate();
	}
	const bool bFinishedPendingSlot = Completed && bAsyncCompletesPendingSlot;
	if (!Completed && bFilesCommitted && bAsyncCompletesPendingSlot) PendingSlotSaveId = CompletedId;
	bAsyncCompletesPendingSlot = false;
	bAsyncWorkerStarted = false;
	bAsyncWorkerCapacityBlocked = false;
	AsyncWorld = nullptr;
	bSaveOperationRunning = false;
	if (bFinishedPendingSlot) CancelPendingSaveSlot();
	LastWorldSaveResult = Completed;
	{
		TGuardValue<bool> Applying(bApplyingSaveCallbacks, true);
		OnWorldSaveFinished.Broadcast(CompletedId, Completed);
	}
	if (DeferredSaveTermination != EPhase::None)
	{
		FlushDeferredSaveTermination();
		return;
	}
	if (bPendingUserQuit) CompleteUserQuit(Completed);
}

bool USaveGameModule::RequestUserQuit(const uint8 InQuitPreference, const bool bInIgnorePlatformRestrictions)
{
	check(IsInGameThread());
	if (bPendingUserQuit) return true;
	bPendingUserQuit = true;
	bQuitNeedsFreshCapture = bSaveOperationRunning;
	PendingQuitPreference = InQuitPreference;
	bPendingQuitIgnorePlatformRestrictions = bInIgnorePlatformRestrictions;
	bQuitWorldWasPaused = GetWorld() && UGameplayStatics::IsGamePaused(GetWorld());
	bQuitSaveModuleWasPaused = GetModuleState() == EModuleState::Paused;
	QuitVoxel = UVoxelModule::GetPtr(GetWorld());
	bQuitVoxelModuleWasPaused = QuitVoxel.IsValid() && QuitVoxel->GetModuleState() == EModuleState::Paused;
	QuitController = GetWorld() ? UGameplayStatics::GetPlayerController(GetWorld(), 0) : nullptr;
	if (QuitController.IsValid())
	{
		QuitController->SetIgnoreMoveInput(true);
		QuitController->SetIgnoreLookInput(true);
	}
	// 暂停菜单也必须能继续真实帧捕获；只恢复保存服务与 Voxel 服务的时钟。
	if (bQuitWorldWasPaused) UGameplayStatics::SetGamePaused(GetWorld(), false);
	if (bQuitSaveModuleWasPaused) UnPause();
	if (bQuitVoxelModuleWasPaused) QuitVoxel->UnPause();
	const auto Result = SaveCurrentSlot();
	if (Result.Code != ESaveResultCode::Accepted) CompleteUserQuit(Result);
	return true;
}

void USaveGameModule::CompleteUserQuit(const FSaveOperationResult& InResult)
{
	check(IsInGameThread());
	if (!bPendingUserQuit || InResult.Code == ESaveResultCode::Accepted) return;
	if (InResult && bQuitNeedsFreshCapture)
	{
		bQuitNeedsFreshCapture = false;
		const auto Next = SaveCurrentSlot();
		if (Next.Code != ESaveResultCode::Accepted) CompleteUserQuit(Next);
		return;
	}
	const auto Profile = InResult ? SaveProfile() : InResult;
	// Profile 的回调可要求停止原世界；延后停止已执行时不能再退出旧 owner。
	if (!bPendingUserQuit || !::IsValid(GetWorld())) return;
	if (!InResult || !Profile)
	{
		UE_LOG(LogTemp, Error, TEXT("User quit retained its live world after save failure: %s"),
			*(InResult ? Profile.Message : InResult.Message).ToString());
		bPendingUserQuit = false;
		if (QuitController.IsValid())
		{
			QuitController->SetIgnoreMoveInput(false);
			QuitController->SetIgnoreLookInput(false);
		}
		if (bQuitVoxelModuleWasPaused && QuitVoxel.IsValid()) QuitVoxel->Pause();
		if (bQuitSaveModuleWasPaused) Pause();
		if (bQuitWorldWasPaused && GetWorld()) UGameplayStatics::SetGamePaused(GetWorld(), true);
		QuitController.Reset();
		QuitVoxel.Reset();
		return;
	}
	bQuitSaveCommitted = true;
	bPendingUserQuit = false;
	UEventModuleStatics::BroadcastEvent<FEventGameExited>(this, { false });
	UKismetSystemLibrary::QuitGame(GetWorld(), QuitController.Get(),
		static_cast<EQuitPreference::Type>(PendingQuitPreference), bPendingQuitIgnorePlatformRestrictions);
}

FSaveOperationResult USaveGameModule::PollPendingSave()
{
	return PollNativeSave(nullptr);
}

FSaveOperationResult USaveGameModule::PollNativeSave(FVoxelTerrainFrameContext* InFrame)
{
	if (!bSaveOperationRunning) return LastWorldSaveResult;
	FSaveOperationResult Result;
	if (AsyncSave && AsyncSave->Poll(Result, InFrame, bSaveTerminationRunning))
	{
		CompleteAsyncSave(Result);
	}
	return bSaveOperationRunning ? FSaveOperationResult::Accepted(AsyncOperationId) : LastWorldSaveResult;
}

FSaveOperationResult USaveGameModule::GetWorldSaveResult(const FGuid& InOperationId) const
{
	check(IsInGameThread());
	if (InOperationId.IsValid() && bSaveOperationRunning && InOperationId == AsyncOperationId)
		return FSaveOperationResult::Accepted(InOperationId);
	if (InOperationId.IsValid() && LastWorldSaveResult.OperationId == InOperationId) return LastWorldSaveResult;
	return FSaveOperationResult::Failed(ESaveResultCode::NotFound, FText::FromString(TEXT("Save operation receipt is unavailable")));
}

void USaveGameModule::BeginPendingSaveSlot(const FCreateSaveSlotParams& Params)
{
	if (IsSaveOperationRunning()) return;
	PendingSlotSaveId.Invalidate();
	PendingSaveSlotParams = Params;
	PendingSaveSlotParams.bCaptureCurrentWorld = true;
	bHasPendingSaveSlot = true;
}

void USaveGameModule::CancelPendingSaveSlot()
{
	if (IsSaveOperationRunning()) return;
	PendingSlotSaveId.Invalidate();
	bHasPendingSaveSlot = false;
	PendingSaveSlotParams = FCreateSaveSlotParams();
}

FSaveOperationResult USaveGameModule::SaveCurrentSlot()
{
	if (bSaveOperationRunning) return FSaveOperationResult::Accepted(AsyncOperationId);
	if (!AsyncCreatedSlotCleanup.IsCompleted())
		return FSaveOperationResult::Failed(ESaveResultCode::Busy, FText::FromString(TEXT("The original failed slot is still being retired")));
	if (bHasPendingSaveSlot)
	{
		if (PendingSlotSaveId.IsValid())
		{
			const auto Result = SaveSlotAsync(PendingSlotSaveId);
			bAsyncCompletesPendingSlot = bool(Result);
			return Result;
		}
		FSaveSlotSummary Summary;
		const FSaveOperationResult Result = CreateSaveSlot(PendingSaveSlotParams, Summary);
		bAsyncCompletesPendingSlot = bool(Result);
		return Result;
	}
	if (ActiveSaveId.IsValid())
	{
		return SaveActiveSlot();
	}
	return FSaveOperationResult::Success();
}

void USaveGameModule::DetachActiveSave()
{
	if (IsSaveOperationRunning()) return;
	ActiveSaveId.Invalidate();
	if (UVoxelModule* Voxel = UVoxelModule::GetPtr(GetWorld()))
	{
		Voxel->SetActiveSaveSource(FGuid(), 0, nullptr);
	}
}

void USaveGameModule::ClearActiveSave()
{
	if (IsSaveOperationRunning()) return;
	DetachActiveSave();
	if(Storage)
	{
		Storage->ClearLastActiveSave();
	}
}

FSaveOperationResult USaveGameModule::LoadSlot(FGuid SaveId, EPhase InPhase)
{
	if (GetWorld() && GetWorld()->GetNetMode() == NM_Client)
	{
		return FSaveOperationResult::Failed(ESaveResultCode::InvalidArgument,
		                                    FText::FromString(TEXT("A guest cannot load a local world into the server session")));
	}
	if (!SaveId.IsValid())
	{
		return FSaveOperationResult::Failed(ESaveResultCode::InvalidArgument, FText::FromString(TEXT("Invalid save id.")));
	}
	if (!Storage || IsSaveOperationRunning())
	{
		return FSaveOperationResult::Failed(ESaveResultCode::Busy, FText::FromString(TEXT("Save system is busy.")));
	}
	TGuardValue<bool> Guard(bSaveOperationRunning, true);
	FSaveManifest Manifest;
	if (!Storage->ReadManifest(SaveId, Manifest) || Manifest.CurrentGeneration <= 0 ||
	    !Storage->IsGenerationComplete(SaveId, Manifest.CurrentGeneration, Manifest))
	{
		return FSaveOperationResult::Failed(ESaveResultCode::NotFound, FText::FromString(TEXT("Committed save slot not found.")));
	}
	const FName CurrentMap(*UGameplayStatics::GetCurrentLevelName(GetWorld(), true));
	if (!Manifest.CurrentMap.IsNone() && Manifest.CurrentMap != CurrentMap)
	{
		PendingLoadContext.SaveId = SaveId;
		PendingLoadContext.Generation = Manifest.CurrentGeneration;
		PendingLoadContext.TargetMap = Manifest.CurrentMap;
		PendingLoadContext.LoadPhase = InPhase;
		if (UWHGameInstance* GameInstance = GetWorld() ? Cast<UWHGameInstance>(GetWorld()->GetGameInstance()) : nullptr)
		{
			GameInstance->SetPendingSaveLoad(PendingLoadContext);
		}
		UGameplayStatics::OpenLevel(this, Manifest.CurrentMap);
		return FSaveOperationResult::Success();
	}
	FPendingSaveLoadContext Context;
	Context.SaveId = SaveId;
	Context.Generation = Manifest.CurrentGeneration;
	Context.TargetMap = Manifest.CurrentMap.IsNone() ? CurrentMap : Manifest.CurrentMap;
	Context.LoadPhase = InPhase;
	return RestoreSlotGeneration(Context, InPhase);
}

FSaveOperationResult USaveGameModule::RestoreSlotGeneration(const FPendingSaveLoadContext& Context, EPhase InPhase)
{
	FSaveOperationResult Result = LoadModulesFromGeneration(Context.SaveId, Context.Generation, InPhase);
	if (Result)
	{
		ActiveSaveId = Context.SaveId;
		if(Storage && !Storage->WriteLastActiveSave(Context.SaveId))
		{
			UE_LOG(LogTemp, Warning, TEXT("Loaded save, but last_active.txt could not be updated."));
		}
		CancelPendingSaveSlot();
	}
	return Result;
}

FSaveOperationResult USaveGameModule::LoadModulesFromGeneration(const FGuid& SaveId, int32 Generation, EPhase InPhase)
{
	struct FLoadedModuleSaveData
	{
		UModuleBase* Module = nullptr;
		FModuleSaveFileHeader Header;
		FParameter Data;
	};
	TArray<FLoadedModuleSaveData> Loaded;
	for (UModuleBase* Module : GetSaveModules(
		ESaveScope::World,
		false))
	{
		TArray<uint8> Bytes;
		FLoadedModuleSaveData Item;
		Item.Module = Module;
		if (!Storage->ReadBinary(Storage->GetModuleFilePath(SaveId, Generation, Module->GetModuleName()), Bytes))
		{
			return FSaveOperationResult::Failed(ESaveResultCode::ReadFailed, FText::FromName(Module->GetModuleName()));
		}
		if (!FSaveDataSerializer::ReadModuleFile(Bytes, Item.Header, Item.Data) || Item.Header.ModuleName != Module->GetModuleName())
		{
			return FSaveOperationResult::Failed(ESaveResultCode::CorruptData, FText::FromName(Module->GetModuleName()));
		}
		if (Item.Header.ModuleVersion != Module->GetSaveDataVersion() && !Module->MigrateSaveData(Item.Header.ModuleVersion, Item.Data))
		{
			return FSaveOperationResult::Failed(ESaveResultCode::VersionMismatch, FText::FromName(Module->GetModuleName()));
		}
		Loaded.Add(MoveTemp(Item));
	}
	for (const FLoadedModuleSaveData& Item : Loaded)
	{
		if (const UVoxelModule* Voxel = Cast<UVoxelModule>(Item.Module))
		{
			FString Error;
			if (!Voxel->ValidateWorldData(Item.Data, Error))
			{
				return FSaveOperationResult::Failed(ESaveResultCode::VersionMismatch, FText::FromString(Error));
			}
		}
	}
	if (PHASEC(InPhase, EPhase::Primary))
	{
		if (UVoxelModule* Voxel = UVoxelModule::GetPtr(GetWorld()))
		{
			Voxel->SetActiveSaveSource(SaveId, Generation, Storage.Get());
		}
		for (FLoadedModuleSaveData& Item : Loaded)
		{
			Item.Module->OnBeforeLoadData();
		}
		for (FLoadedModuleSaveData& Item : Loaded)
		{
			Item.Module->LoadSaveData(Item.Data, EPhase::Primary);
			RestoreSceneEnvironment(Item.Module, Item.Data, EPhase::Primary);
		}
	}
	if (PHASEC(InPhase, EPhase::Lesser))
	{
		for (FLoadedModuleSaveData& Item : Loaded)
		{
			Item.Module->LoadSaveData(Item.Data, EPhase::Lesser);
			RestoreSceneEnvironment(Item.Module, Item.Data, EPhase::Lesser);
		}
	}
	if (PHASEC(InPhase, EPhase::Final))
	{
		for (FLoadedModuleSaveData& Item : Loaded)
		{
			Item.Module->LoadSaveData(Item.Data, EPhase::Final);
			RestoreSceneEnvironment(Item.Module, Item.Data, EPhase::Final);
		}
		for (FLoadedModuleSaveData& Item : Loaded)
		{
			Item.Module->OnAfterLoadData(true);
		}
	}
	return FSaveOperationResult::Success();
}

FSaveOperationResult USaveGameModule::DeleteSaveSlot(FGuid SaveId)
{
	if (IsSaveOperationRunning())
	{
		return FSaveOperationResult::Failed(ESaveResultCode::Busy, FText::FromString(TEXT("Save transaction is in progress")));
	}
	if (!Storage || !SaveId.IsValid())
	{
		return FSaveOperationResult::Failed(ESaveResultCode::InvalidArgument, FText::FromString(TEXT("Invalid save id.")));
	}
	if (Storage->BeginWorldWrite(SaveId, nullptr, {}) != ESaveWorldWriteResult::Acquired)
		return FSaveOperationResult::Failed(ESaveResultCode::Busy, FText::FromString(TEXT("Save slot is owned by another native writer")));
	ON_SCOPE_EXIT { Storage->EndWorldWrite(); };

	FSaveManifest ExistingManifest;
	Storage->ReadManifest(SaveId, ExistingManifest);
	const bool bWasActive = SaveId == ActiveSaveId;
	FGuid PersistedSaveId;
	const bool bWasPersistedLast = Storage->ReadLastActiveSave(PersistedSaveId) && PersistedSaveId == SaveId;
	if(bWasActive)
	{
		DetachActiveSave();
	}

	if(!Storage->DeleteWorldDirectory(SaveId))
	{
		if(bWasActive)
		{
			ActiveSaveId = SaveId;
			if(UVoxelModule* Voxel = UVoxelModule::GetPtr(GetWorld()))
			{
				Voxel->SetActiveSaveSource(SaveId, ExistingManifest.CurrentGeneration, Storage.Get());
			}
		}
		return FSaveOperationResult::Failed(ESaveResultCode::WriteFailed, FText::FromString(TEXT("Delete save slot failed.")));
	}

	if(bWasPersistedLast)
	{
		Storage->ClearLastActiveSave();
	}
	return FSaveOperationResult::Success();
}

FSaveOperationResult USaveGameModule::RenameSaveSlot(FGuid SaveId, const FString& NewName)
{
	if (IsSaveOperationRunning())
	{
		return FSaveOperationResult::Failed(ESaveResultCode::Busy, FText::FromString(TEXT("Save transaction is in progress")));
	}
	if (!Storage || !SaveId.IsValid() || NewName.IsEmpty())
	{
		return FSaveOperationResult::Failed(ESaveResultCode::InvalidArgument, FText::FromString(TEXT("Invalid save slot or name.")));
	}
	if (Storage->BeginWorldWrite(SaveId, nullptr, {}) != ESaveWorldWriteResult::Acquired)
		return FSaveOperationResult::Failed(ESaveResultCode::Busy, FText::FromString(TEXT("Save slot is owned by another native writer")));
	ON_SCOPE_EXIT { Storage->EndWorldWrite(); };
	FSaveManifest Manifest;
	if (!Storage->ReadManifest(SaveId, Manifest))
		return FSaveOperationResult::Failed(ESaveResultCode::NotFound, FText::FromString(TEXT("Save slot is unavailable")));
	Manifest.DisplayName = NewName;
	Manifest.UpdatedAt = FDateTime::UtcNow();
	return Storage->WriteManifestAtomic(SaveId, Manifest)
	           ? FSaveOperationResult::Success()
	           : FSaveOperationResult::Failed(ESaveResultCode::WriteFailed, FText::FromString(TEXT("Rename save slot failed.")));
}

FSaveOperationResult USaveGameModule::DuplicateSaveSlot(FGuid SourceSaveId, const FString& NewName, FSaveSlotSummary& OutSummary)
{
	if (IsSaveOperationRunning())
	{
		return FSaveOperationResult::Failed(ESaveResultCode::Busy, FText::FromString(TEXT("Save transaction is in progress")));
	}
	FSaveManifest Manifest;
	if (!Storage || NewName.IsEmpty() || !Storage->ReadManifest(SourceSaveId, Manifest) || Manifest.CurrentGeneration <= 0 ||
	    !Storage->IsGenerationComplete(SourceSaveId, Manifest.CurrentGeneration, Manifest))
	{
		return FSaveOperationResult::Failed(ESaveResultCode::InvalidArgument, FText::FromString(TEXT("Invalid source save slot.")));
	}
	const auto SourceLease = FSaveGameGenerationReadLease::Acquire(Storage->GetGenerationDir(SourceSaveId, Manifest.CurrentGeneration));
	if (!SourceLease || !Storage->IsGenerationComplete(SourceSaveId, Manifest.CurrentGeneration, Manifest))
		return FSaveOperationResult::Failed(ESaveResultCode::ReadFailed, FText::FromString(TEXT("Source generation cannot be pinned")));
	const FGuid TargetSaveId = FGuid::NewGuid();
	if (Storage->BeginWorldWrite(TargetSaveId, nullptr, {}) != ESaveWorldWriteResult::Acquired)
		return FSaveOperationResult::Failed(ESaveResultCode::Busy, FText::FromString(TEXT("Duplicate slot is owned by another native writer")));
	ON_SCOPE_EXIT { Storage->EndWorldWrite(); };
	if (!Storage->CreateWorldDirectory(TargetSaveId) ||
	    !Storage->CopyCurrentGeneration(SourceSaveId, TargetSaveId, Manifest.CurrentGeneration, Manifest.CurrentGeneration))
	{
		Storage->DeleteWorldDirectory(TargetSaveId);
		return FSaveOperationResult::Failed(ESaveResultCode::WriteFailed, FText::FromString(TEXT("Duplicate generation failed.")));
	}
	const FString SourcePreview = FPaths::Combine(Storage->GetWorldDir(SourceSaveId), Manifest.PreviewFile);
	const FString TargetPreview = FPaths::Combine(Storage->GetWorldDir(TargetSaveId), Manifest.PreviewFile);
	if (IFileManager::Get().FileExists(*SourcePreview))
	{
		IFileManager::Get().Copy(*TargetPreview, *SourcePreview, true, true);
	}
	Manifest.SaveId = TargetSaveId;
	Manifest.DisplayName = NewName;
	Manifest.CreatedAt = FDateTime::UtcNow();
	Manifest.UpdatedAt = Manifest.CreatedAt;
	if (!Storage->WriteManifestAtomic(TargetSaveId, Manifest))
	{
		Storage->DeleteWorldDirectory(TargetSaveId);
		return FSaveOperationResult::Failed(ESaveResultCode::WriteFailed, FText::FromString(TEXT("Duplicate manifest failed.")));
	}
	OutSummary = Manifest.ToSummary(Storage->GetWorldDir(TargetSaveId));
	return FSaveOperationResult::Success();
}

FSaveSlotSummary USaveGameModule::GetSaveSlotSummary(FGuid SaveId) const
{
	FSaveManifest Manifest;
	if (!Storage || !Storage->ReadManifest(SaveId, Manifest) || Manifest.CurrentGeneration <= 0 ||
	    !Storage->IsGenerationComplete(SaveId, Manifest.CurrentGeneration, Manifest))
	{
		return FSaveSlotSummary();
	}
	return Manifest.ToSummary(Storage->GetWorldDir(SaveId));
}

TArray<FSaveSlotSummary> USaveGameModule::GetSaveSlotSummaries() const
{
	TArray<FSaveSlotSummary> Result;
	TArray<FSaveManifest> Manifests;
	if (Storage && Storage->EnumerateManifests(Manifests))
	{
		for (const FSaveManifest& Manifest : Manifests)
		{
			if (Manifest.CurrentGeneration > 0 && Storage->IsGenerationComplete(Manifest.SaveId, Manifest.CurrentGeneration, Manifest))
			{
				Result.Add(Manifest.ToSummary(Storage->GetWorldDir(Manifest.SaveId)));
			}
		}
		Result.Sort(
		    [](const FSaveSlotSummary& A, const FSaveSlotSummary& B)
		    {
			    return A.UpdatedAt > B.UpdatedAt;
		    });
	}
	return Result;
}

FSaveOperationResult USaveGameModule::SaveProfile(FName ProfileName)
{
	if (IsSaveOperationRunning())
	{
		return FSaveOperationResult::Failed(ESaveResultCode::Busy, FText::FromString(TEXT("Save transaction is in progress")));
	}
	USettingModule* Setting = USettingModule::GetPtr(GetWorld());
	if (!Setting || !Storage)
	{
		return FSaveOperationResult::Failed(ESaveResultCode::ModuleMissing, FText::FromString(TEXT("SettingModule missing.")));
	}
	const TStrongObjectPtr<USettingModule> SettingOwner(Setting);
	FSaveOperationResult Result;
	{
		TGuardValue<bool> Applying(bApplyingSaveCallbacks, true);
		Setting->OnBeforeSaveData();
		TArray<uint8> Bytes;
		const bool bSaved = ::IsValid(Setting) && Setting->BuildSaveFile(Bytes) && ::IsValid(Setting) &&
			Storage->WriteBinaryAtomic(Storage->GetProfileModuleFilePath(ProfileName, Setting->GetModuleName()), Bytes);
		if (::IsValid(Setting)) Setting->OnAfterSaveData(bSaved);
		Result = bSaved ? FSaveOperationResult::Success() :
			FSaveOperationResult::Failed(ESaveResultCode::WriteFailed, FText::FromString(TEXT("Profile save failed.")));
	}
	FlushDeferredSaveTermination();
	return Result;
}

FSaveOperationResult USaveGameModule::LoadProfile(FName ProfileName)
{
	if (IsSaveOperationRunning())
	{
		return FSaveOperationResult::Failed(ESaveResultCode::Busy, FText::FromString(TEXT("Save transaction is in progress")));
	}
	USettingModule* Setting = USettingModule::GetPtr(GetWorld());
	TArray<uint8> Bytes;
	FModuleSaveFileHeader Header;
	FParameter Data;
	if (!Setting || !Storage || !Storage->ReadBinary(Storage->GetProfileModuleFilePath(ProfileName, Setting->GetModuleName()), Bytes))
	{
		return FSaveOperationResult::Failed(Setting ? ESaveResultCode::NotFound : ESaveResultCode::ModuleMissing,
		                                    FText::FromString(TEXT("Profile not found.")));
	}
	if (!FSaveDataSerializer::ReadModuleFile(Bytes, Header, Data) || Header.ModuleName != Setting->GetModuleName())
	{
		return FSaveOperationResult::Failed(ESaveResultCode::CorruptData, FText::FromString(TEXT("Profile data is corrupt.")));
	}
	const TStrongObjectPtr<USettingModule> SettingOwner(Setting);
	FSaveOperationResult Result;
	{
		TGuardValue<bool> Applying(bApplyingSaveCallbacks, true);
		if (Header.ModuleVersion != Setting->GetSaveDataVersion() && !Setting->MigrateSaveData(Header.ModuleVersion, Data))
		{
			Result = FSaveOperationResult::Failed(ESaveResultCode::VersionMismatch, FText::FromString(TEXT("Profile version mismatch.")));
		}
		else if (::IsValid(Setting))
		{
			Setting->OnBeforeLoadData();
			if (::IsValid(Setting)) Setting->LoadSaveData(Data, EPhase::Primary);
			if (::IsValid(Setting)) Setting->LoadSaveData(Data, EPhase::Final);
			if (::IsValid(Setting)) Setting->OnAfterLoadData(true);
			Result = ::IsValid(Setting) ? FSaveOperationResult::Success() :
				FSaveOperationResult::Failed(ESaveResultCode::ModuleMissing, FText::FromString(TEXT("Profile load lost its original setting module owner.")));
		}
		else Result = FSaveOperationResult::Failed(ESaveResultCode::ModuleMissing,
			FText::FromString(TEXT("Profile migration lost its original setting module owner.")));
	}
	FlushDeferredSaveTermination();
	return Result;
}

TArray<UModuleBase*> USaveGameModule::GetSaveModules(
	const ESaveScope Scope,
	const bool bRequireSaveEnabled) const
{
	TArray<UModuleBase*> Result;
	if (AMainModule* MainModule = AMainModule::GetPtr(GetWorld()))
	{
		for (UModuleBase* Module : MainModule->GetModules())
		{
			if (Module &&
				Module->GetSaveScope() == Scope &&
				(!bRequireSaveEnabled || Module->IsSaveEnabled()))
			{
				Result.Add(Module);
			}
		}
	}
	Result.Sort(
	    [](const UModuleBase& A, const UModuleBase& B)
	    {
		    return A.GetModuleIndex() < B.GetModuleIndex();
	    });
	return Result;
}

void USaveGameModule::OnGameExited(UObject* InSender, const FEventGameExited& InEvent)
{
	if(InEvent.bIsSimulating || bQuitSaveCommitted)
	{
		return;
	}

	const FSaveOperationResult SaveResult = SaveCurrentSlot();
	if(!SaveResult)
	{
		UE_LOG(LogTemp, Error, TEXT("Saving before exit failed: %s"), *SaveResult.Message.ToString());
	}
	SaveProfile();
}

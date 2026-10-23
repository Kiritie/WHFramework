#include "SaveGame/SaveGameModule.h"

#include "Event/EventModuleStatics.h"
#include "Event/Events/Common/Game/Event_GameExited.h"
#include "Gameplay/WHGameInstance.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "GameFramework/PlayerController.h"
#include "Main/MainModule.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
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
TAutoConsoleVariable<int32> CVarSaveStepTimeLimit(TEXT("wh.SaveGame.EnforceStepTimeLimit"), 1,
	TEXT("Reject save steps that exceed the configured time limit. 0 records overruns without rejecting saves."), ECVF_Default);

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
        if (Plan->ExternalOperation)
        {
            FString Error;
            Plan->ExternalOperation->Store->CloseNative();
            if (!OwnerStorage.AbortPendingWorld(Plan->ExternalOperation->PendingLease, Error))
                UE_LOG(LogTemp, Error, TEXT("未发布世界分支保留恢复 intent：%s"), *Error);
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

	if(Storage->ReadLastActiveSave(ActiveSaveId) && !Storage->HasPendingWorld(ActiveSaveId))
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
	if (const UWHGameInstance* GameInstance = GetWorld() ? Cast<UWHGameInstance>(GetWorld()->GetGameInstance()) : nullptr)
	{
		LocalSaveContext = GameInstance->GetLocalSaveContext();
		if (!LocalSaveContext)
		{
			UE_LOG(LogTemp, Error, TEXT("Local save module rejected: %s"), *GameInstance->GetLocalSaveContextError());
			return;
		}
	}
	Storage = MakeUnique<FSaveGameStorage>(UserIndex, LocalSaveContext ? LocalSaveContext->GetStorageRoot() : FString());
    Storage->EnsureRoot();
    TArray<FString> RecoveryErrors;
    Storage->RecoverPendingWorlds(RecoveryErrors);
    for (const FString& Error : RecoveryErrors) UE_LOG(LogTemp, Warning, TEXT("世界分支恢复：%s"), *Error);
	Storage->CleanupAllTempGenerations();
	RestoreLastActiveSave();
	if (UWHGameInstance* GameInstance = GetWorld() ? Cast<UWHGameInstance>(GetWorld()->GetGameInstance()) : nullptr)
	{
		LoadSnapshot = GameInstance->ConsumePendingSaveLoad();
		if (LoadSnapshot.IsValid())
		{
			LoadOperationResult = FSaveOperationResult::Accepted(LoadSnapshot.Context.OperationId);
		}
	}
	UEventModuleStatics::SubscribeEvent<FEventGameExited>(this, &ThisClass::OnGameExited);
}

void USaveGameModule::OnPreparatory(EPhase InPhase)
{
	Super::OnPreparatory(InPhase);
	if (PHASEC(InPhase, EPhase::Final) && LoadSnapshot.IsValid() && LoadSnapshot.AppliedPhases == EPhase::None)
	{
		const FPendingSaveLoadContext Context = LoadSnapshot.Context;
		LoadOperationResult = RestoreLoadSnapshot(Context.LoadPhase);
		LoadOperationResult.OperationId = Context.OperationId;
		FlushDeferredSaveTermination();
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
        if (bSaveOperationRunning && bAsyncWorkerStarted && AsyncSave && AsyncSave->IsRunning() && !AsyncCompletionResult.IsSet())
        {
            AsyncSave->RequestCancel();
            DeferredSaveTermination = EPhase(uint8(DeferredSaveTermination) | uint8(InPhase));
            return;
        }
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
			LoadSnapshot = FSaveGameLoadSnapshot();
			Storage.Reset();
			LocalSaveContext.Reset();
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
    if (AsyncCompletionResult.IsSet() && DeferredSaveTermination != EPhase::None)
        AdvanceAsyncSaveCompletion(nullptr, true);
    else if (!bCaptureAdvanced && AsyncCompletionResult.IsSet() && !AsyncVoxelOwner) AdvanceAsyncSaveCompletion();
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
	if (!LocalSaveContext && GetWorld() && Cast<UWHGameInstance>(GetWorld()->GetGameInstance())) return;
	DetachActiveSave();
	UserIndex = InUserIndex;
	Storage = MakeUnique<FSaveGameStorage>(UserIndex, LocalSaveContext ? LocalSaveContext->GetStorageRoot() : FString());
    Storage->EnsureRoot();
    TArray<FString> RecoveryErrors;
    Storage->RecoverPendingWorlds(RecoveryErrors);
    for (const FString& Error : RecoveryErrors) UE_LOG(LogTemp, Warning, TEXT("世界分支恢复：%s"), *Error);
	Storage->CleanupAllTempGenerations();
	RestoreLastActiveSave();
}

FSaveOperationResult USaveGameModule::CreateSaveSlot(const FCreateSaveSlotParams& Params, FSaveSlotSummary& OutSummary)
{
    OutSummary = {};
    if (!Storage || IsSaveOperationRunning() || !GetWorld() || GetWorld()->GetNetMode() == NM_Client)
        return FSaveOperationResult::Failed(ESaveResultCode::Busy, FText::FromString(TEXT("当前不能创建世界存档。")));
    FSaveManifest Manifest;
    Manifest.SaveId = FGuid::NewGuid();
    Manifest.DisplayName = Params.DisplayName;
    Manifest.Description = Params.Description;
    Manifest.CreatedAt = FDateTime::UtcNow();
    Manifest.UpdatedAt = Manifest.CreatedAt;
    Manifest.CurrentMap = Params.InitialMap;
    if (Params.bCaptureCurrentWorld)
    {
        PendingCreatedSaveId = Manifest.SaveId;
        PendingCreatedManifest = Manifest;
        PendingCreationPreviousSaveId = ActiveSaveId;
        const FSaveOperationResult Result = SaveSlotAsync(Manifest.SaveId);
        if (!Result)
        {
            PendingCreatedSaveId.Invalidate();
            PendingCreatedManifest.Reset();
            PendingCreationPreviousSaveId.Invalidate();
            return Result;
        }
        OutSummary = Manifest.ToSummary(Storage->GetWorldDir(Manifest.SaveId));
        OutSummary.bValid = false;
        return Result;
    }
    TGuardValue<bool> Guard(bSaveOperationRunning, true);
    if (Storage->BeginWorldWrite(Manifest.SaveId, nullptr, {}) != ESaveWorldWriteResult::Acquired)
        return FSaveOperationResult::Failed(ESaveResultCode::Busy, FText::FromString(TEXT("目标槽正在被另一个写入者使用。")));
    ON_SCOPE_EXIT { Storage->EndWorldWrite(); };
    if (!Storage->CreateWorldDirectory(Manifest.SaveId) || !Storage->WriteManifestAtomic(Manifest.SaveId, Manifest))
    {
        Storage->DeleteWorldDirectory(Manifest.SaveId);
        return FSaveOperationResult::Failed(ESaveResultCode::WriteFailed, FText::FromString(TEXT("空存档槽创建失败。")));
    }
    ActiveSaveId = Manifest.SaveId;
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
    if (Storage->HasPendingWorld(SaveId))
    {
        TArray<FString> Errors;
        Storage->RecoverPendingWorlds(Errors);
        if (Storage->HasPendingWorld(SaveId))
            return FSaveOperationResult::Failed(ESaveResultCode::ConfirmationPending,
                FText::FromString(TEXT("目标存档仍有未确认分支，必须先恢复或确认该操作。")));
    }
    FSaveManifest TargetManifest;
    const bool bCreateAtWrite = PendingCreatedSaveId == SaveId && PendingCreatedManifest.IsSet();
    if (bCreateAtWrite) TargetManifest = PendingCreatedManifest.GetValue();
    else if (!Storage->ReadManifest(SaveId, TargetManifest))
        return FSaveOperationResult::Failed(ESaveResultCode::NotFound, FText::FromString(TEXT("目标存档槽不存在。")));
    if (TargetManifest.CurrentGeneration == MAX_int32)
        return FSaveOperationResult::Failed(ESaveResultCode::CommitFailed, FText::FromString(TEXT("存档代次已达到上限。")));
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
	Plan.LocalSaveContext = LocalSaveContext;
    Plan.Manifest = TargetManifest;
    Plan.OperationId = OperationId;
    Plan.bCreateSlotAtWrite = bCreateAtWrite;
    Plan.bResolveManifestOnWorker = false;
	Plan.bWriteLastActiveSave = true;
	Plan.CurrentMap = FName(*UGameplayStatics::GetCurrentLevelName(GetWorld(), true));
    Plan.bDeleteCreatedSlotOnFailure = PendingCreatedSaveId == SaveId;
    FSaveWorldCaptureContext Context;
    Context.OperationId = OperationId;
    Context.SourceSaveId = ActiveSaveId;
    Context.TargetSaveId = SaveId;
    Context.StorageRoot = Storage->GetRootDir();
    Context.TargetGeneration = TargetManifest.CurrentGeneration + 1;
    Context.bDeleteWorldOnAbort = bCreateAtWrite;
    FString ExternalError;
    if (!BeginExternalStore(Context, {}, AsyncStoreParticipant, AsyncExternalOperation, ExternalError))
    {
        AsyncPlan.Reset();
        AsyncCaptureMetadataLease.Reset();
        return FSaveOperationResult::Failed(TargetManifest.CurrentGeneration > 0 ? ESaveResultCode::AlreadyExists :
            ESaveResultCode::CaptureFailed, FText::FromString(ExternalError));
    }
    Plan.ExternalOperation = AsyncExternalOperation;
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
	const FGuid CaptureOperation = AsyncOperationId;
	const int32 CapturePhase = AsyncModuleCapturePhase;
	const int32 CaptureIndex = bPreparationPending ? AsyncPrepareCount : AsyncBuildCursor;
	const UModuleBase* CaptureModule = AsyncCaptured.IsValidIndex(CaptureIndex) ? AsyncCaptured[CaptureIndex] : nullptr;
	const FName CaptureModuleName = ::IsValid(CaptureModule) ? CaptureModule->GetModuleName() : NAME_None;
	const TCHAR* CaptureStage = bPreparationPending ? TEXT("BeforeSaveData") :
		VoxelOwner && !AsyncPlan->Voxel.IsSet() ? TEXT("CopyVoxelSnapshot") :
		VoxelOwner && !AsyncPlan->Voxel->Voxels.Completion->ExpectedStorageRoot ? TEXT("BindVoxelCommitOwner") :
		AsyncBuildCursor < AsyncCaptured.Num() ? TEXT("ModuleFile") : TEXT("StartWorker");
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
	else if (VoxelOwner && !AsyncPlan->Voxel.IsSet())
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
	else if (Failure && VoxelOwner && AsyncPlan->Voxel.IsSet() &&
		!AsyncPlan->Voxel->Voxels.Completion->ExpectedStorageRoot)
	{
		if (!VoxelOwner->BindSaveCommitOwner(AsyncSaveId, AsyncPlan->StorageRoot, Error))
		{
			bCapacityBlocked = Error.StartsWith(TEXT("CapacityBlocked:"));
			if (!bCapacityBlocked) Failure = FSaveOperationResult::Failed(ESaveResultCode::CaptureFailed, FText::FromString(Error));
		}
	}
	else if (Failure && !bCapacityBlocked && AsyncBuildCursor < AsyncCaptured.Num())
	{
		TGuardValue<bool> Applying(bApplyingSaveCallbacks, true);
		if (!AdvanceModuleFileCapture(Error, bCapacityBlocked) && !bCapacityBlocked)
			Failure = FSaveOperationResult::Failed(ESaveResultCode::SerializeFailed, FText::FromString(Error));
	}
	else if (Failure && !bCapacityBlocked)
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
	if (Actual >= 0.25 || !Failure)
	{
		UE_LOG(LogTemp, Display, TEXT("Save capture slice: operation=%s stage=%s module=%s phase=%d duration=%.3fms capacityBlocked=%s"),
			*CaptureOperation.ToString(), CaptureStage, *CaptureModuleName.ToString(), CapturePhase, Actual,
			bCapacityBlocked ? TEXT("true") : TEXT("false"));
	}
	AsyncCaptureCostMilliseconds = FMath::Lerp(AsyncCaptureCostMilliseconds, Actual, 0.15);
	if (InFrame)
	{
		InFrame->GetPublishBudget().Finish(EVoxelPublishPhase::Prepare, Started, Finished);
		if (VoxelOwner && Actual > VoxelOwner->GetMaximumSceneCaptureStepMilliseconds() && AsyncPlan)
		{
			const FString TimingError = FString::Printf(
				TEXT("Save module capture step capability error: stage=%s module=%s phase=%d %.3fms exceeds %.3fms"),
				CaptureStage, *CaptureModuleName.ToString(), CapturePhase, Actual, VoxelOwner->GetMaximumSceneCaptureStepMilliseconds());
			if (IsStepTimeLimitEnforced()) Failure = FSaveOperationResult::Failed(ESaveResultCode::CaptureFailed, FText::FromString(TimingError));
			else UE_LOG(LogTemp, Warning, TEXT("Save timing limit disabled: %s"), *TimingError);
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
        if (AsyncExternalOperation && AsyncStoreParticipant.Transform &&
            !AsyncStoreParticipant.Transform(AsyncExternalOperation->Context, AsyncExternalOperation->Store,
                Module->GetModuleName(), AsyncModuleData, OutError)) return false;
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
	bAsyncCompletionFilesCommitted = bool(Result) || (AsyncExternalOperation && AsyncExternalOperation->bFilesCommitted.load());
	AsyncCompletionCursor = -1;
}

void USaveGameModule::AdvanceAsyncSaveCompletion(FVoxelTerrainFrameContext* InFrame, const bool bForce)
{
	check(IsInGameThread());
	if (!AsyncCompletionResult.IsSet() || bApplyingSaveCallbacks) return;
	UVoxelModule* const VoxelOwner = AsyncVoxelOwner;
	if (!bForce && VoxelOwner && (!InFrame || !InFrame->IsActive())) return;
	const int32 CompletionCursor = AsyncCompletionCursor;
	const FString CompletionStage = CompletionCursor < 0 ? TEXT("ReleaseCapture") :
		CompletionCursor < AsyncPrepareCount ? GetNameSafe(AsyncCaptured[CompletionCursor]) : TEXT("FinalizeSave");
	const double Started = FPlatformTime::Seconds();
	if (!bForce && InFrame &&
		!InFrame->GetPublishBudget().TryStart(EVoxelPublishPhase::Retire, Started, AsyncCompletionCostMilliseconds))
	{
		if (!InFrame->IsOverdueOpportunityAvailable()) return;
		InFrame->ConsumeOverdueOpportunity();
	}
	if (AsyncCompletionCursor < 0)
	{
        if (!bAsyncCompletionFilesCommitted && !bAsyncWorkerStarted && AsyncPlan &&
            (PendingCreatedSaveId == AsyncSaveId || AsyncExternalOperation))
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
		}
        if (bAsyncCompletionFilesCommitted && AsyncVoxelOwner && Module == AsyncVoxelOwner &&
            (!::IsValid(AsyncVoxelOwner) || !AsyncVoxelOwner->DidApplySaveCommit()))
        {
            const FString Error = TEXT("存档文件已提交，但原体素世界没有接受目标基线；必须重新加载目标存档。");
            AsyncCompletionResult->bSuccess = false;
            AsyncCompletionResult->Code = ESaveResultCode::CommitFailed;
            AsyncCompletionResult->Message = FText::FromString(Error);
            if (AsyncExternalOperation)
            {
                AsyncExternalOperation->Context.bProjectionFailed = true;
                AsyncExternalOperation->Context.ProjectionFailure = Error;
            }
        }
	}
	else FinalizeAsyncSave();
	const double Finished = FPlatformTime::Seconds();
	const double Actual = (Finished - Started) * 1000.0;
	UE_LOG(LogTemp, Display, TEXT("Save completion slice: operation=%s stage=%s cursor=%d duration=%.3fms committed=%s"),
		*AsyncOperationId.ToString(), *CompletionStage, CompletionCursor, Actual, bAsyncCompletionFilesCommitted ? TEXT("true") : TEXT("false"));
	AsyncCompletionCostMilliseconds = FMath::Lerp(AsyncCompletionCostMilliseconds, Actual, 0.15);
	if (!bForce && InFrame)
	{
		InFrame->GetPublishBudget().Finish(EVoxelPublishPhase::Retire, Started, Finished);
		if (VoxelOwner && Actual > VoxelOwner->GetMaximumSceneCaptureStepMilliseconds())
		{
			const FString Error = FString::Printf(TEXT("Save completion capability error: %s[%d] %.3fms exceeds %.3fms"),
				*CompletionStage, CompletionCursor, Actual, VoxelOwner->GetMaximumSceneCaptureStepMilliseconds());
			if (!IsStepTimeLimitEnforced())
			{
				UE_LOG(LogTemp, Warning, TEXT("Save timing limit disabled: %s"), *Error);
			}
			else if (AsyncCompletionResult.IsSet())
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
    if (AsyncExternalOperation)
    {
        FString Error;
        TGuardValue<bool> Applying(bApplyingSaveCallbacks, true);
        if (!FinishExternalStore(AsyncStoreParticipant, AsyncExternalOperation, bAsyncCompletionFilesCommitted, Error))
        {
            AsyncCompletionResult->bSuccess = false;
            AsyncCompletionResult->Code = AsyncExternalOperation->Context.bPublicationUncertain ?
                ESaveResultCode::ConfirmationPending : ESaveResultCode::CommitFailed;
            AsyncCompletionResult->Message = FText::FromString(Error);
        }
    }
    const bool bPublicationUncertain = AsyncExternalOperation && AsyncExternalOperation->Context.bPublicationUncertain;
    AsyncExternalOperation.Reset();
    AsyncStoreParticipant = {};
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
		if (!bFilesCommitted && !bPublicationUncertain)
		{
			ActiveSaveId = PendingCreationPreviousSaveId;
		}
		PendingCreatedSaveId.Invalidate();
        PendingCreatedManifest.Reset();
		PendingCreationPreviousSaveId.Invalidate();
	}
    if (bPublicationUncertain) ActiveSaveId.Invalidate();
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
	if (AsyncSave && AsyncSave->Poll(Result, InFrame, bSaveTerminationRunning || DeferredSaveTermination != EPhase::None))
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
	LoadSnapshot = FSaveGameLoadSnapshot();
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
	const FGuid OperationId = !PHASEC(InPhase, EPhase::Primary) && LoadSnapshot.IsValid() && LoadSnapshot.Context.SaveId == SaveId
		? LoadSnapshot.Context.OperationId : FGuid::NewGuid();
	return RequestLoadSlot(SaveId, InPhase, OperationId);
}

FSaveOperationResult USaveGameModule::GetLoadOperationResult(const FGuid& InOperationId) const
{
	if (InOperationId.IsValid() && LoadOperationResult.OperationId == InOperationId)
	{
		return LoadOperationResult;
	}
	FSaveOperationResult Result;
	Result.Code = ESaveResultCode::None;
	return Result;
}

void USaveGameModule::CancelLoadOperation(const FGuid& InOperationId)
{
	if (!InOperationId.IsValid())
	{
		return;
	}
	if (UWHGameInstance* GameInstance = GetWorld() ? Cast<UWHGameInstance>(GetWorld()->GetGameInstance()) : nullptr)
	{
		GameInstance->CancelPendingSaveLoad(InOperationId);
	}
	const bool bOwnsSnapshot = LoadSnapshot.Context.OperationId == InOperationId;
	if (bOwnsSnapshot)
	{
		LoadSnapshot = FSaveGameLoadSnapshot();
	}
	if (bOwnsSnapshot || (LoadOperationResult.OperationId == InOperationId && LoadOperationResult.Code == ESaveResultCode::Accepted))
	{
		LoadOperationResult = FSaveOperationResult::Failed(ESaveResultCode::Cancelled, NSLOCTEXT("WH.SaveGame", "LoadCancelled", "存档加载请求已取消。"));
		LoadOperationResult.OperationId = InOperationId;
	}
}

FSaveOperationResult USaveGameModule::RequestLoadSlot(FGuid SaveId, EPhase InPhase, const FGuid& InOperationId)
{
	ON_SCOPE_EXIT { FlushDeferredSaveTermination(); };
	if (!InOperationId.IsValid())
	{
		return FSaveOperationResult::Failed(ESaveResultCode::InvalidArgument, NSLOCTEXT("WH.SaveGame", "InvalidLoadOperation", "存档加载请求缺少有效操作身份。"));
	}
	const auto Reject = [&InOperationId](ESaveResultCode InCode, const FText& InMessage)
	{
		FSaveOperationResult Result = FSaveOperationResult::Failed(InCode, InMessage);
		Result.OperationId = InOperationId;
		return Result;
	};
	if (LoadOperationResult.Code == ESaveResultCode::Accepted)
	{
		return Reject(ESaveResultCode::Busy, NSLOCTEXT("WH.SaveGame", "LoadAlreadyPending", "已有存档切图请求等待完成。"));
	}
	if (GetWorld() && GetWorld()->GetNetMode() == NM_Client)
	{
		return Reject(ESaveResultCode::InvalidArgument,
		                                    FText::FromString(TEXT("A guest cannot load a local world into the server session")));
	}
	if (!SaveId.IsValid())
	{
		return Reject(ESaveResultCode::InvalidArgument, FText::FromString(TEXT("Invalid save id.")));
	}
	if (!Storage || IsSaveOperationRunning())
	{
		return Reject(ESaveResultCode::Busy, FText::FromString(TEXT("Save system is busy.")));
	}
	if (!PHASEC(InPhase, EPhase::Primary))
	{
		if (!LoadSnapshot.IsValid() || LoadSnapshot.Context.SaveId != SaveId || LoadSnapshot.Context.OperationId != InOperationId)
		{
			return Reject(ESaveResultCode::InvalidArgument, NSLOCTEXT("WH.SaveGame", "MissingLoadSnapshot", "存档后续阶段缺少匹配的加载快照。"));
		}
		LoadOperationResult = RestoreLoadSnapshot(InPhase);
		LoadOperationResult.OperationId = InOperationId;
		return LoadOperationResult;
	}
    if (Storage->HasPendingWorld(SaveId))
    {
        TArray<FString> Errors;
        Storage->RecoverPendingWorlds(Errors);
        if (Storage->HasPendingWorld(SaveId))
            return Reject(ESaveResultCode::ConfirmationPending,
                FText::FromString(TEXT("此槽的分支发布仍待确认，当前禁止加载。")));
    }
	TGuardValue<bool> Guard(bSaveOperationRunning, true);
	FSaveManifest Manifest;
	if (!Storage->ReadManifest(SaveId, Manifest) || Manifest.CurrentGeneration <= 0 ||
	    !Storage->IsGenerationComplete(SaveId, Manifest.CurrentGeneration, Manifest))
	{
		return Reject(ESaveResultCode::NotFound, FText::FromString(TEXT("Committed save slot not found.")));
	}
	const FName CurrentMap(*UGameplayStatics::GetCurrentLevelName(GetWorld(), true));
	FSaveGameLoadSnapshot Snapshot;
	Snapshot.Context.OperationId = InOperationId;
	Snapshot.Context.SaveId = SaveId;
	Snapshot.Context.Generation = Manifest.CurrentGeneration;
	Snapshot.Context.TargetMap = Manifest.CurrentMap.IsNone() ? CurrentMap : Manifest.CurrentMap;
	Snapshot.Context.LoadPhase = InPhase;
	Snapshot.ReadLease = FSaveGameGenerationReadLease::Acquire(Storage->GetGenerationDir(SaveId, Manifest.CurrentGeneration));
	if (!Snapshot.CanApply(InPhase) || !Storage->IsGenerationComplete(SaveId, Manifest.CurrentGeneration, Manifest))
	{
		return Reject(ESaveResultCode::ReadFailed, NSLOCTEXT("WH.SaveGame", "LoadSnapshotUnavailable", "无法锁定完整的存档文件代。"));
	}
	if (!Manifest.CurrentMap.IsNone() && FName(*FPackageName::GetShortName(Manifest.CurrentMap.ToString())) != CurrentMap)
	{
		UWHGameInstance* GameInstance = GetWorld() ? Cast<UWHGameInstance>(GetWorld()->GetGameInstance()) : nullptr;
		if (!GameInstance)
		{
			return Reject(ESaveResultCode::ModuleMissing, NSLOCTEXT("WH.SaveGame", "LoadGameInstanceUnavailable", "跨地图存档加载所需的游戏实例不可用。"));
		}
		if (!GameInstance->SetPendingSaveLoad(MoveTemp(Snapshot), this))
		{
			return Reject(ESaveResultCode::Busy, NSLOCTEXT("WH.SaveGame", "PendingLoadOccupied", "游戏实例已有等待接手的存档请求。"));
		}
		LoadOperationResult = FSaveOperationResult::Accepted(InOperationId);
		UGameplayStatics::OpenLevel(this, Manifest.CurrentMap);
		return LoadOperationResult;
	}
	LoadSnapshot = MoveTemp(Snapshot);
	LoadOperationResult = RestoreLoadSnapshot(InPhase);
	LoadOperationResult.OperationId = InOperationId;
	return LoadOperationResult;
}

FSaveOperationResult USaveGameModule::RestoreLoadSnapshot(EPhase InPhase)
{
	const FSaveGameLoadSnapshot Snapshot = LoadSnapshot;
	const FPendingSaveLoadContext& Context = Snapshot.Context;
	const FName CurrentMap(*UGameplayStatics::GetCurrentLevelName(GetWorld(), true));
	if (!Storage || !Snapshot.CanApply(InPhase) ||
		FName(*FPackageName::GetShortName(Context.TargetMap.ToString())) != CurrentMap ||
		!Snapshot.ReadLease->Matches(Storage->GetGenerationDir(Context.SaveId, Context.Generation)))
	{
		LoadSnapshot = FSaveGameLoadSnapshot();
		return FSaveOperationResult::Failed(ESaveResultCode::InvalidArgument,
			NSLOCTEXT("WH.SaveGame", "InvalidLoadSnapshot", "存档加载快照、目标地图或阶段不匹配。"));
	}
	TGuardValue<bool> Guard(bSaveOperationRunning, true);
	TGuardValue<bool> Applying(bApplyingSaveCallbacks, true);
	FSaveOperationResult Result = LoadModulesFromGeneration(Context.SaveId, Context.Generation, InPhase);
	if (LoadSnapshot.Context.OperationId != Context.OperationId || DeferredSaveTermination != EPhase::None)
	{
		LoadSnapshot = FSaveGameLoadSnapshot();
		return FSaveOperationResult::Failed(ESaveResultCode::Cancelled, NSLOCTEXT("WH.SaveGame", "LoadCancelled", "存档加载请求已取消。"));
	}
	if (Result)
	{
		LoadSnapshot.AppliedPhases = EPhase(uint8(LoadSnapshot.AppliedPhases) | uint8(InPhase));
		ActiveSaveId = Context.SaveId;
		if(Storage && !Storage->WriteLastActiveSave(Context.SaveId))
		{
			UE_LOG(LogTemp, Warning, TEXT("Loaded save, but last_active.txt could not be updated."));
		}
		CancelPendingSaveSlot();
	}
	if (!Result || PHASEC(InPhase, EPhase::Final))
	{
		LoadSnapshot = FSaveGameLoadSnapshot();
	}
	return Result;
}

FSaveOperationResult USaveGameModule::LoadModulesFromGeneration(const FGuid& SaveId, int32 Generation, EPhase InPhase)
{
	const FGuid OperationId = LoadSnapshot.Context.OperationId;
	const auto IsCancelled = [this, OperationId]()
	{
		return LoadSnapshot.Context.OperationId != OperationId || DeferredSaveTermination != EPhase::None;
	};
	const auto Cancelled = []()
	{
		return FSaveOperationResult::Failed(ESaveResultCode::Cancelled,
			NSLOCTEXT("WH.SaveGame", "LoadCancelled", "存档加载请求已取消。"));
	};
    if (!Storage || Storage->HasPendingWorld(SaveId))
        return FSaveOperationResult::Failed(ESaveResultCode::ConfirmationPending,
            FText::FromString(TEXT("世界文件代尚未完成分支确认，不能应用其模块。")));
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
		if (IsCancelled()) return Cancelled();
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
    TMap<FName, FParameter> GuardModules;
    for(const FLoadedModuleSaveData& Item : Loaded) GuardModules.Add(Item.Module->GetModuleName(), Item.Data);
    FString GuardError;
    if(!RunWorldLoadGuards(SaveId, GuardModules, InPhase, false, GuardError))
    {
        return FSaveOperationResult::Failed(ESaveResultCode::LoadFailed, FText::FromString(GuardError));
    }
	if (IsCancelled()) return Cancelled();
	if (PHASEC(InPhase, EPhase::Primary))
	{
		if (UVoxelModule* Voxel = UVoxelModule::GetPtr(GetWorld()))
		{
			Voxel->SetActiveSaveSource(SaveId, Generation, Storage.Get());
		}
		for (FLoadedModuleSaveData& Item : Loaded)
		{
			Item.Module->OnBeforeLoadData();
			if (IsCancelled()) return Cancelled();
		}
		for (FLoadedModuleSaveData& Item : Loaded)
		{
			Item.Module->LoadSaveData(Item.Data, EPhase::Primary);
			if (IsCancelled()) return Cancelled();
			RestoreSceneEnvironment(Item.Module, Item.Data, EPhase::Primary);
			if (IsCancelled()) return Cancelled();
		}
	}
	if (PHASEC(InPhase, EPhase::Lesser))
	{
		for (FLoadedModuleSaveData& Item : Loaded)
		{
			Item.Module->LoadSaveData(Item.Data, EPhase::Lesser);
			if (IsCancelled()) return Cancelled();
			RestoreSceneEnvironment(Item.Module, Item.Data, EPhase::Lesser);
			if (IsCancelled()) return Cancelled();
		}
	}
	if (PHASEC(InPhase, EPhase::Final))
	{
		for (FLoadedModuleSaveData& Item : Loaded)
		{
			Item.Module->LoadSaveData(Item.Data, EPhase::Final);
			if (IsCancelled()) return Cancelled();
			RestoreSceneEnvironment(Item.Module, Item.Data, EPhase::Final);
			if (IsCancelled()) return Cancelled();
		}
	}
    if(!RunWorldLoadGuards(SaveId, GuardModules, InPhase, true, GuardError))
    {
        if(PHASEC(InPhase, EPhase::Final))
        {
            for(FLoadedModuleSaveData& Item : Loaded) Item.Module->OnAfterLoadData(false);
        }
        return FSaveOperationResult::Failed(ESaveResultCode::LoadFailed, FText::FromString(GuardError));
    }
	if(PHASEC(InPhase, EPhase::Final))
	{
		for(FLoadedModuleSaveData& Item : Loaded)
		{
			if (IsCancelled()) return Cancelled();
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

    if (WorldStoreParticipant.BeforeDelete)
    {
        FString Error;
        FSaveWorldStoreDelete BeforeDelete = WorldStoreParticipant.BeforeDelete;
        TGuardValue<bool> Applying(bApplyingSaveCallbacks, true);
        if (!BeforeDelete(SaveId, Error))
            return FSaveOperationResult::Failed(ESaveResultCode::Busy, FText::FromString(Error));
    }
	FSaveManifest ExistingManifest;
	const bool bHadManifest = Storage->ReadManifest(SaveId, ExistingManifest);
	const bool bWasActive = SaveId == ActiveSaveId;
	FGuid PersistedSaveId;
	const bool bWasPersistedLast = Storage->ReadLastActiveSave(PersistedSaveId) && PersistedSaveId == SaveId;
	if(bWasActive)
	{
		DetachActiveSave();
	}

	if(!Storage->DeleteWorldDirectory(SaveId))
	{
        // 退休 rename 可能已经成功，只有仍能验证原清单和代次才恢复源绑定。
        FSaveManifest RetainedManifest;
        const bool bOriginalAvailable = bHadManifest && Storage->ReadManifest(SaveId, RetainedManifest) &&
            RetainedManifest.SaveId == SaveId && RetainedManifest.CurrentGeneration == ExistingManifest.CurrentGeneration &&
            RetainedManifest.LastCommitOperationId == ExistingManifest.LastCommitOperationId &&
            (RetainedManifest.CurrentGeneration == 0 ||
                Storage->IsGenerationComplete(SaveId, RetainedManifest.CurrentGeneration, RetainedManifest));
		if(bWasActive && bOriginalAvailable)
		{
			ActiveSaveId = SaveId;
			if(UVoxelModule* Voxel = UVoxelModule::GetPtr(GetWorld()))
			{
				Voxel->SetActiveSaveSource(SaveId, RetainedManifest.CurrentGeneration, Storage.Get());
			}
		}
        if (bWasPersistedLast && !bOriginalAvailable) Storage->ClearLastActiveSave();
		return FSaveOperationResult::Failed(bOriginalAvailable ? ESaveResultCode::WriteFailed : ESaveResultCode::ConfirmationPending,
            FText::FromString(bOriginalAvailable ? TEXT("删除失败，原存档路径仍然完整。") :
                TEXT("存档可能已经退出可加载目录，但删除持久结果尚未确认；活动绑定保持解除，请重新读取存档列表。")));
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
    check(IsInGameThread());
    OutSummary = {};
    if (IsSaveOperationRunning() || !Storage || !GetWorld() || GetWorld()->GetNetMode() == NM_Client)
        return FSaveOperationResult::Failed(ESaveResultCode::Busy, FText::FromString(TEXT("当前不能复制存档。")));
    if (!SourceSaveId.IsValid() || NewName.IsEmpty())
        return FSaveOperationResult::Failed(ESaveResultCode::InvalidArgument, FText::FromString(TEXT("源存档或新名称无效。")));
    TSharedPtr<FSaveWorldSourceLease, ESPMode::ThreadSafe> NativeSourceLease;
    FString SourceError;
    if (!Storage->AcquireWorldSourceLease(SourceSaveId, NativeSourceLease, SourceError))
        return FSaveOperationResult::Failed(ESaveResultCode::Busy, FText::FromString(SourceError));
    FSaveManifest Manifest;
    if (Storage->HasPendingWorld(SourceSaveId) || !Storage->ReadManifest(SourceSaveId, Manifest) ||
        Manifest.CurrentGeneration <= 0 || Manifest.ModuleVersions.IsEmpty() || Manifest.ModuleVersions.Num() > 256 ||
        !Storage->IsGenerationComplete(SourceSaveId, Manifest.CurrentGeneration, Manifest))
        return FSaveOperationResult::Failed(ESaveResultCode::InvalidArgument, FText::FromString(TEXT("源存档无完整可复制代次。")));
    ON_SCOPE_EXIT { FlushDeferredSaveTermination(); };
    TGuardValue<bool> Running(bSaveOperationRunning, true);
    TGuardValue<bool> Applying(bApplyingSaveCallbacks, true);
    const FString SourceDirectory = Storage->GetGenerationDir(SourceSaveId, Manifest.CurrentGeneration);
    const auto SourceLease = FSaveGameGenerationReadLease::Acquire(SourceDirectory);
    if (!SourceLease || !Storage->IsGenerationComplete(SourceSaveId, Manifest.CurrentGeneration, Manifest))
        return FSaveOperationResult::Failed(ESaveResultCode::ReadFailed, FText::FromString(TEXT("源存档代次无法固定。")));

    // 普通模块完整验证后才允许外部存储准备；只复制纯 DTO，不加载或卸载当前世界。
    TMap<FName, FParameter> SourceModules;
    TArray<FName> ModuleNames;
    Manifest.ModuleVersions.GetKeys(ModuleNames);
    ModuleNames.Sort(FNameLexicalLess());
    int64 TotalModuleBytes = 0;
    for (const FName SourceModuleName : ModuleNames)
    {
        const FString Name = SourceModuleName.ToString();
        if (SourceModuleName.IsNone() || Name.Contains(TEXT("/")) || Name.Contains(TEXT("\\")) || Name.Contains(TEXT("..")))
            return FSaveOperationResult::Failed(ESaveResultCode::CorruptData, FText::FromString(TEXT("源模块名称不能包含路径。")));
        const FString Path = Storage->GetModuleFilePath(SourceSaveId, Manifest.CurrentGeneration, SourceModuleName);
        const int64 Size = IFileManager::Get().FileSize(*Path);
        if (Size <= 0 || Size > 512ll * 1024 * 1024 || TotalModuleBytes > 512ll * 1024 * 1024 - Size)
            return FSaveOperationResult::Failed(ESaveResultCode::CorruptData, FText::FromString(TEXT("复制源模块总长度无效或超出 512 MiB。")));
        TotalModuleBytes += Size;
        TArray<uint8> Bytes;
        FModuleSaveFileHeader Header;
        FParameter Data;
        if (!Storage->ReadBinary(Path, Bytes) || !FSaveDataSerializer::ReadModuleFile(Bytes, Header, Data) ||
            Header.ModuleName != SourceModuleName || Header.ModuleVersion != Manifest.ModuleVersions[SourceModuleName])
            return FSaveOperationResult::Failed(ESaveResultCode::CorruptData, FText::FromString(TEXT("源模块校验失败：") + SourceModuleName.ToString()));
        SourceModules.Add(SourceModuleName, MoveTemp(Data));
    }
    FSaveWorldCaptureContext Context;
    Context.OperationId = FGuid::NewGuid();
    Context.SourceSaveId = SourceSaveId;
    Context.SourceGeneration = Manifest.CurrentGeneration;
    Context.TargetSaveId = FGuid::NewGuid();
    OutSummary.SaveId = Context.TargetSaveId;
    Context.TargetGeneration = Manifest.CurrentGeneration;
    Context.StorageRoot = Storage->GetRootDir();
    Context.bDuplicate = true;
    Context.bDeleteWorldOnAbort = true;
    FSaveWorldStoreParticipant Participant;
    TSharedPtr<FSaveWorldExternalOperation, ESPMode::ThreadSafe> External;
    FString Error;
    if (!BeginExternalStore(Context, SourceModules, Participant, External, Error))
        return FSaveOperationResult::Failed(ESaveResultCode::CaptureFailed, FText::FromString(Error));
    if (External) Context = External->Context;
    bool bPublished = false;
    bool bFinished = false;
    bool bWriteOwned = false;
    auto Finish = [&]()
    {
        if (bFinished) return;
        bFinished = true;
        FString FinishError;
        if (External)
        {
            FinishExternalStore(Participant, External, bPublished, FinishError);
            External->Store->CloseNative();
            const bool bCleaned = External->Context.bPublicationUncertain ? false :
                (bPublished ? Storage->CompletePendingWorld(External->PendingLease, FinishError) :
                    Storage->AbortPendingWorld(External->PendingLease, FinishError));
            if (!bCleaned || !FinishError.IsEmpty()) UE_LOG(LogTemp, Warning, TEXT("复制存档保留安全恢复信息：%s"), *FinishError);
        }
        else if (!bPublished && bWriteOwned)
        {
            FSaveManifest Observed;
            if (!Storage->ReadManifest(Context.TargetSaveId, Observed) || Observed.CurrentGeneration == 0)
                Storage->DeleteWorldDirectory(Context.TargetSaveId);
        }
        if (bWriteOwned) Storage->EndWorldWrite();
    };
    ON_SCOPE_EXIT { Finish(); };
    auto Fail = [&](const ESaveResultCode Code, const FString& Message)
    {
        FSaveOperationResult Result = FSaveOperationResult::Failed(Code, FText::FromString(Message));
        Result.OperationId = Context.OperationId;
        return Result;
    };
    if (Storage->BeginWorldWrite(Context.TargetSaveId, nullptr, {}) != ESaveWorldWriteResult::Acquired)
        return Fail(ESaveResultCode::Busy, TEXT("目标存档已被另一个写入者持有。"));
    bWriteOwned = true;
    if (!Storage->CreateWorldDirectory(Context.TargetSaveId))
        return Fail(ESaveResultCode::WriteFailed, TEXT("复制目标目录创建失败。"));
    if (External && (!External->Store->PrepareNative(Error) || !External->Store->MarkReadyNative(Error)))
        return Fail(ESaveResultCode::WriteFailed, Error);
    if (!Storage->PrepareTempGeneration(Context.TargetSaveId, 0, Context.TargetGeneration, &SourceDirectory, &SourceLease))
        return Fail(ESaveResultCode::WriteFailed, TEXT("源体素/场景基线复制失败。"));
    for (const FName SourceModuleName : ModuleNames)
    {
        FParameter Data = SourceModules[SourceModuleName];
        if (External && !Participant.Transform(Context, External->Store, SourceModuleName, Data, Error))
            return Fail(ESaveResultCode::SerializeFailed, Error);
        TArray<uint8> Bytes;
        if (!FSaveDataSerializer::BuildModuleFile(SourceModuleName, Manifest.ModuleVersions[SourceModuleName], Data, Bytes) ||
            !Storage->WriteBinary(Storage->GetTempModuleFilePath(Context.TargetSaveId, Context.TargetGeneration, SourceModuleName), Bytes))
            return Fail(ESaveResultCode::WriteFailed, TEXT("复制模块写入失败：") + SourceModuleName.ToString());
    }
    if (!Storage->FlushTempGeneration(Context.TargetSaveId, Context.TargetGeneration) ||
        !Storage->CommitGeneration(Context.TargetSaveId, Context.TargetGeneration))
        return Fail(ESaveResultCode::WriteFailed, TEXT("目标普通文件代未持久发布。"));
    if (!Manifest.PreviewFile.IsEmpty() && FPaths::GetCleanFilename(Manifest.PreviewFile) == Manifest.PreviewFile)
    {
        const FString SourcePreview = FPaths::Combine(Storage->GetWorldDir(SourceSaveId), Manifest.PreviewFile);
        const int64 PreviewSize = IFileManager::Get().FileSize(*SourcePreview);
        if (PreviewSize > 0 && PreviewSize <= 16 * 1024 * 1024)
        {
            TArray<uint8> Preview;
            if (!Storage->ReadBinary(SourcePreview, Preview) ||
                !Storage->WriteBinaryAtomic(FPaths::Combine(Storage->GetWorldDir(Context.TargetSaveId), Manifest.PreviewFile), Preview))
                return Fail(ESaveResultCode::WriteFailed, TEXT("复制预览文件失败。"));
        }
    }
    Manifest.SaveId = Context.TargetSaveId;
    Manifest.LastCommitOperationId = Context.OperationId;
    Manifest.DisplayName = NewName;
    Manifest.CreatedAt = FDateTime::UtcNow();
    Manifest.UpdatedAt = Manifest.CreatedAt;
    if (!Storage->WriteManifestAtomic(Context.TargetSaveId, Manifest, External ? External->PendingLease :
        TSharedPtr<FSavePendingWorldLease, ESPMode::ThreadSafe>()))
    {
        FSaveManifest Observed;
        const bool bObserved = Storage->ReadManifest(Context.TargetSaveId, Observed);
        const bool bMatches = bObserved && Observed.LastCommitOperationId == Context.OperationId &&
            Observed.CurrentGeneration == Context.TargetGeneration;
        if (!bObserved || (bMatches && !Storage->IsGenerationComplete(Context.TargetSaveId, Context.TargetGeneration, Observed)))
        {
            if (External) External->Context.bPublicationUncertain = true;
            return Fail(ESaveResultCode::ConfirmationPending, TEXT("复制目标的发布结果暂时不可确认，完整目标与恢复 intent 已保留。"));
        }
        if (!bMatches) return Fail(ESaveResultCode::CommitFailed, TEXT("复制目标 Manifest 未发布。"));
        if (!Storage->ConfirmManifestCommit(Context.TargetSaveId, Context.OperationId, Context.TargetGeneration, Error))
        {
            if (External) External->Context.bPublicationUncertain = true;
            return Fail(ESaveResultCode::ConfirmationPending, Error);
        }
        Manifest = MoveTemp(Observed);
    }
    bPublished = true;
    if (External) External->bFilesCommitted.store(true);
    OutSummary = Manifest.ToSummary(Storage->GetWorldDir(Context.TargetSaveId));
    Finish();
    FSaveOperationResult Result = FSaveOperationResult::Success();
    Result.OperationId = Context.OperationId;
    return Result;
}

FSaveSlotSummary USaveGameModule::GetSaveSlotSummary(FGuid SaveId) const
{
	FSaveManifest Manifest;
	if (!Storage || Storage->HasPendingWorld(SaveId) || !Storage->ReadManifest(SaveId, Manifest) || Manifest.CurrentGeneration <= 0 ||
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
			if (Manifest.CurrentGeneration > 0 && !Storage->HasPendingWorld(Manifest.SaveId) && Storage->IsGenerationComplete(Manifest.SaveId, Manifest.CurrentGeneration, Manifest))
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


bool USaveGameModule::RegisterWorldLoadGuard(FName InName, FSaveWorldLoadGuard InBefore, FSaveWorldLoadGuard InAfter)
{
    if(!IsInGameThread() || InName.IsNone() || !InBefore || !InAfter || WorldLoadGuards.Contains(InName)) return false;
    FWorldLoadGuardEntry Entry;
    Entry.Before = MoveTemp(InBefore);
    Entry.After = MoveTemp(InAfter);
    WorldLoadGuards.Add(InName, MoveTemp(Entry));
    return true;
}

void USaveGameModule::UnregisterWorldLoadGuard(FName InName)
{
    check(IsInGameThread());
    WorldLoadGuards.Remove(InName);
}

bool USaveGameModule::RunWorldLoadGuards(const FGuid& InSaveId, const TMap<FName, FParameter>& InModules, EPhase InPhase, bool bInAfter, FString& OutError)
{
    TArray<FName> Names;
    WorldLoadGuards.GetKeys(Names);
    Names.Sort(FNameLexicalLess());
    for(FName Name : Names)
    {
        FWorldLoadGuardEntry* Entry = WorldLoadGuards.Find(Name);
        if(!Entry) continue;
        FSaveWorldLoadGuard Guard = bInAfter ? Entry->After : Entry->Before;
        if(!Guard(InSaveId, InModules, InPhase, OutError))
        {
            if(OutError.IsEmpty()) OutError = FString::Printf(TEXT("World load guard failed: %s"), *Name.ToString());
            return false;
        }
    }
    return true;
}

bool USaveGameModule::RegisterWorldStoreParticipant(FName InName, FSaveWorldStoreParticipant InParticipant)
{
    check(IsInGameThread());
    if (InName.IsNone() || !WorldStoreParticipantName.IsNone() || !InParticipant.Begin ||
        !InParticipant.Transform || !InParticipant.Finish || !InParticipant.BeforeDelete) return false;
    WorldStoreParticipantName = InName;
    WorldStoreParticipant = MoveTemp(InParticipant);
    return true;
}

void USaveGameModule::UnregisterWorldStoreParticipant(FName InName)
{
    check(IsInGameThread());
    if (InName == WorldStoreParticipantName)
    {
        WorldStoreParticipantName = NAME_None;
        WorldStoreParticipant = {};
    }
}

FString USaveGameModule::GetStorageRoot() const
{
    return Storage ? Storage->GetRootDir() : FString();
}

bool USaveGameModule::IsStepTimeLimitEnforced()
{
	return CVarSaveStepTimeLimit.GetValueOnGameThread() != 0;
}

FString USaveGameModule::GetUnboundInventoryRoot() const
{
	return LocalSaveContext ? LocalSaveContext->GetUnboundInventoryRoot() :
		FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Inventory"), TEXT("Unbound"));
}

bool USaveGameModule::BeginExternalStore(FSaveWorldCaptureContext InContext,
    const TMap<FName, FParameter>& InSourceModules, FSaveWorldStoreParticipant& OutParticipant,
    TSharedPtr<FSaveWorldExternalOperation, ESPMode::ThreadSafe>& OutOperation, FString& OutError)
{
    check(IsInGameThread());
    OutOperation.Reset();
    OutParticipant = WorldStoreParticipant;
    if (!OutParticipant.Begin) return true;
    TGuardValue<bool> Applying(bApplyingSaveCallbacks, true);
    TSharedPtr<ISaveWorldExternalStore, ESPMode::ThreadSafe> Prepared;
    if (!OutParticipant.Begin(InContext, InSourceModules, Prepared, OutError)) return false;
    if (!Prepared) return true;
    FSavePendingWorld Intent;
    Intent.OperationId = InContext.OperationId;
    Intent.TargetSaveId = InContext.TargetSaveId;
    Intent.SourceSaveId = InContext.SourceSaveId;
    Intent.SourceGeneration = InContext.SourceGeneration;
    Intent.TargetGeneration = InContext.TargetGeneration;
    Intent.WorldId = Prepared->GetWorldId();
    Intent.StoreId = Prepared->GetStoreId();
    Intent.bDeleteWorldOnAbort = InContext.bDeleteWorldOnAbort;
    TSharedPtr<FSavePendingWorldLease, ESPMode::ThreadSafe> Lease;
    if (!Storage->BeginPendingWorld(Intent, Lease, OutError))
    {
        FString FinishError;
        OutParticipant.Finish(InContext, Prepared, false, FinishError);
        Prepared->CloseNative();
        if (!FinishError.IsEmpty()) OutError += TEXT("; ") + FinishError;
        return false;
    }
    OutOperation = MakeShared<FSaveWorldExternalOperation, ESPMode::ThreadSafe>();
    OutOperation->Context = MoveTemp(InContext);
    OutOperation->Store = MoveTemp(Prepared);
    OutOperation->PendingLease = MoveTemp(Lease);
    return true;
}

bool USaveGameModule::FinishExternalStore(const FSaveWorldStoreParticipant& InParticipant,
    const TSharedPtr<FSaveWorldExternalOperation, ESPMode::ThreadSafe>& InOperation,
    const bool bInFilesCommitted, FString& OutError)
{
    check(IsInGameThread());
    if (!InOperation || !InOperation->Store) return true;
    return InParticipant.Finish && InParticipant.Finish(InOperation->Context, InOperation->Store, bInFilesCommitted, OutError);
}

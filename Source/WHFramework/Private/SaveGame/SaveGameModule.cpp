#include "SaveGame/SaveGameModule.h"
#include "SaveGame/SaveOperationTypes.h"
#include "WHFrameworkCoreMacros.h"
#include "SaveGame/SaveSlotTypes.h"
#include "SaveGame/SaveManifestTypes.h"
#include "SaveGame/SaveFileTypes.h"
#include "Common/LifecycleTypes.h"
#include "Common/NativeTaskRetirement.h"

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
#include "SaveGame/SaveGameCaptureOperation.h"
#include "Setting/SettingModule.h"
#include "Scene/SceneModule.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/Task/VoxelTerrainResources.h"

IMPLEMENTATION_MODULE(USaveGameModule)

namespace
{
TAutoConsoleVariable<int32> CVarSaveStepTimeLimit(TEXT("wh.SaveGame.EnforceStepTimeLimit"), 1,
	TEXT("Reject save steps that exceed the configured time limit. 0 records overruns without rejecting saves."), ECVF_Default);

bool IsValidProfileName(FName ProfileName)
{
	if (ProfileName.IsNone()) return false;
	const FString Name = ProfileName.ToString();
	if (Name.IsEmpty() || Name.Len() > 64) return false;
	for (TCHAR Character : Name)
	{
		if (!FChar::IsAlnum(Character) && Character != TEXT('_') && Character != TEXT('-')) return false;
	}
	const FString Upper = Name.ToUpper();
	if (Upper == TEXT("CON") || Upper == TEXT("PRN") || Upper == TEXT("AUX") || Upper == TEXT("NUL")) return false;
	if (Upper.Len() == 4 && (Upper.StartsWith(TEXT("COM")) || Upper.StartsWith(TEXT("LPT"))) && Upper[3] >= TEXT('1') && Upper[3] <= TEXT('9')) return false;
	return true;
}

FSaveOperationResult TransientSessionUnsupported()
{
	return FSaveOperationResult::Failed(ESaveResultCode::Unsupported,
		NSLOCTEXT("WH.SaveGame", "TransientSessionUnsupported", "当前关卡是临时会话，不读取或写入游戏存档。"));
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
}

#if WITH_EDITOR
void USaveGameModule::OnDestroy()
{
	Super::OnDestroy();
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

void USaveGameModule::LockSessionPolicy()
{
	if (bSessionPolicyLocked) return;
	RuntimeSessionPolicy = SessionPolicy;
	bSessionPolicyLocked = true;
}

void USaveGameModule::OnInitialize()
{
	// 先固定配置再调用蓝图事件；重入/重复初始化不能重置已接纳的存档操作。
	if (bSessionPolicyLocked) return;
	LockSessionPolicy();
	Super::OnInitialize();
	UWHGameInstance* GameInstance = GetWorld() ? Cast<UWHGameInstance>(GetWorld()->GetGameInstance()) : nullptr;
	if (IsTransientSession())
	{
		ActiveSaveId.Invalidate();
		bHasPendingSaveSlot = false;
		PendingSaveSlotParams = FCreateSaveSlotParams();
		LoadSnapshot = FSaveGameLoadSnapshot();
		Storage.Reset();
		LocalSaveContext.Reset();
		// 拒绝跨图快照并保留同一操作身份；消费后立即释放文件代读取租约。
		if (GameInstance) AdoptPendingSaveLoad(GameInstance->ConsumePendingSaveLoad());
		UEventModuleStatics::SubscribeEvent<FEventGameExited>(this, &ThisClass::OnGameExited);
		return;
	}
	if (GameInstance)
	{
		if (!GameInstance->EnsureLocalSaveContext())
		{
			UE_LOG(LogTemp, Error, TEXT("Local save module rejected: %s"), *GameInstance->GetLocalSaveContextError());
			return;
		}
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
	if (GameInstance) AdoptPendingSaveLoad(GameInstance->ConsumePendingSaveLoad());
	UEventModuleStatics::SubscribeEvent<FEventGameExited>(this, &ThisClass::OnGameExited);
}

void USaveGameModule::AdoptPendingSaveLoad(FSaveGameLoadSnapshot InSnapshot)
{
	if (!InSnapshot.IsValid()) return;
	const FGuid OperationId = InSnapshot.Context.OperationId;
	if (IsTransientSession())
	{
		LoadOperationResult = TransientSessionUnsupported();
		LoadOperationResult.OperationId = OperationId;
		return;
	}
	LoadSnapshot = MoveTemp(InSnapshot);
	LoadOperationResult = FSaveOperationResult::Accepted(OperationId);
}

void USaveGameModule::OnPreparatory(EPhase InPhase)
{
	Super::OnPreparatory(InPhase);
	if (PHASEC(InPhase, EPhase::Final) && LoadSnapshot.IsValid() && LoadSnapshot.AppliedPhases == EPhase::None)
	{
		const FPendingSaveLoadContext Context = LoadSnapshot.Context;
		LoadOperationResult = RestoreLoadSnapshot(Context.LoadPhase);
		LoadOperationResult.OperationId = Context.OperationId;
	}
}

void USaveGameModule::PrepareForHostTermination()
{
    bHostTerminationRequested = true;
    if (!bSaveOperationRunning || !AsyncOperationId.IsValid()) return;

    TGuardValue<bool> Terminating(bSaveTerminationRunning, true);
    bPendingUserQuit = false;
    if (AsyncSave) AsyncSave->RequestCancel();
    while (AsyncSave && AsyncSave->IsRunning() && !AsyncCompletionResult.IsSet())
    {
        AsyncSave->WaitForNativeExit(1000);
        PollNativeSave(nullptr);
    }
    if (!AsyncCompletionResult.IsSet())
    {
        CompleteAsyncSave(FSaveOperationResult::Failed(ESaveResultCode::Cancelled,
            FText::FromString(TEXT("World termination cancelled the unfinished save capture"))));
    }
    while (AsyncCompletionResult.IsSet()) AdvanceAsyncSaveCompletion(nullptr, true);
}

void USaveGameModule::OnTermination(EPhase InPhase)
{
    if (!bHostTerminationRequested) PrepareForHostTermination();
    Super::OnTermination(InPhase);
    if (PHASEC(InPhase, EPhase::Final))
    {
        OnSaveTerminationComplete();
        AsyncSave.Reset();
        AsyncExecutorLease.Reset();
        LoadSnapshot = FSaveGameLoadSnapshot();
        Storage.Reset();
        LocalSaveContext.Reset();
    }
}
void USaveGameModule::OnRefresh(float DeltaSeconds, bool bInEditor)
{
	Super::OnRefresh(DeltaSeconds, bInEditor);
}

void USaveGameModule::OnControlRefresh(float RealDeltaSeconds)
{
	Super::OnControlRefresh(RealDeltaSeconds);
	if (AsyncCreatedSlotCleanup.IsCompleted()) AsyncCreatedSlotCleanup = {};
	if (!bSaveOperationRunning)
	{
		return;
	}
	const bool bLostVoxelOwner = AsyncVoxelOwner && !::IsValid(AsyncVoxelOwner);
	if (AsyncVoxelOwner && !bLostVoxelOwner)
	{
		return;
	}
	if (bLostVoxelOwner && !AsyncCompletionResult.IsSet())
	{
		if (bAsyncWorkerStarted && AsyncSave)
		{
			if (AsyncSave->IsRunning()) AsyncSave->RequestCancel();
			PollNativeSave(nullptr);
			if (!AsyncCompletionResult.IsSet()) return;
		}
	}
	if (bLostVoxelOwner && !AsyncCompletionResult.IsSet())
	{
		CompleteAsyncSave(FSaveOperationResult::Failed(ESaveResultCode::CaptureFailed,
			FText::FromString(TEXT("Save capture lost its original voxel module owner"))));
	}
	const bool bCaptureAdvanced = AsyncPlan.IsValid() && !AsyncCompletionResult.IsSet();
	if (bCaptureAdvanced) AdvanceAsyncSaveCapture();
	PollNativeSave(nullptr);
    if (AsyncCompletionResult.IsSet() && bLostVoxelOwner)
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
	if (!IsBusinessAdmissionOpen()) return;
	if (IsSaveOperationRunning())
	{
		return;
	}
	if (IsTransientSession())
	{
		UserIndex = InUserIndex;
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
	if (!IsBusinessAdmissionOpen()) return FSaveOperationResult::Failed(ESaveResultCode::Cancelled, FText::FromString(TEXT("原世界正在终止。")));
	if (IsTransientSession()) return TransientSessionUnsupported();
	if (bHasPendingSaveSlot && !bPendingSlotAdoptionInProgress)
		return FSaveOperationResult::Failed(ESaveResultCode::Busy, FText::FromString(TEXT("已有待创建存档，必须先处理原请求。")));
	FString RequiredError;
	if (!CheckRequiredWorldParticipants(RequiredError)) return FSaveOperationResult::Failed(ESaveResultCode::ModuleMissing, FText::FromString(RequiredError));
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
	if (!IsBusinessAdmissionOpen()) return FSaveOperationResult::Failed(ESaveResultCode::Cancelled, FText::FromString(TEXT("原世界正在终止。")));
	if (IsTransientSession()) return TransientSessionUnsupported();
	return ActiveSaveId.IsValid() ? SaveSlot(ActiveSaveId) : FSaveOperationResult::Failed(ESaveResultCode::NotFound, FText::FromString(TEXT("No active save")));
}

FSaveOperationResult USaveGameModule::SaveSlotAsync(FGuid SaveId)
{
	check(IsInGameThread());
	if (!IsBusinessAdmissionOpen()) return FSaveOperationResult::Failed(ESaveResultCode::Cancelled, FText::FromString(TEXT("原世界正在终止。")));
	if (IsTransientSession()) return TransientSessionUnsupported();
	if (bHasPendingSaveSlot && !bPendingSlotAdoptionInProgress)
		return FSaveOperationResult::Failed(ESaveResultCode::Busy, FText::FromString(TEXT("已有待创建存档，必须先处理原请求。")));
	FString RequiredError;
	if (!CheckRequiredWorldParticipants(RequiredError)) return FSaveOperationResult::Failed(ESaveResultCode::ModuleMissing, FText::FromString(RequiredError));
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
			VoxelBoundedAllocation::TaskBytes<FNativeTaskRetirementSignal>() + VoxelBoundedAllocation::WorkerSharedBytes<FEventRef>() +
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

FSaveOperationResult USaveGameModule::SaveActiveSlotAsync()
{
	if (!IsBusinessAdmissionOpen()) return FSaveOperationResult::Failed(ESaveResultCode::Cancelled, FText::FromString(TEXT("原世界正在终止。")));
	if (IsTransientSession()) return TransientSessionUnsupported();
	return ActiveSaveId.IsValid() ? SaveSlotAsync(ActiveSaveId)
	                              : FSaveOperationResult::Failed(ESaveResultCode::NotFound, FText::FromString(TEXT("No active save")));
}

bool USaveGameModule::RequestUserQuit(APlayerController* InPlayer, const uint8 InQuitPreference, const bool bInIgnorePlatformRestrictions)
{
	check(IsInGameThread());
	if (!IsBusinessAdmissionOpen() || bHostTerminationRequested || !::IsValid(InPlayer) || !InPlayer->IsLocalController() || InPlayer->GetWorld() != GetWorld()) return false;
	if (IsTransientSession())
	{
		TWeakObjectPtr<UWorld> World = GetWorld();
		if (!World.IsValid()) return false;
		TWeakObjectPtr<APlayerController> Controller = InPlayer;
		UEventModuleStatics::BroadcastEvent<FEventGameExited>(this, { false });
		if (World.IsValid() && !World->bIsTearingDown && Controller.IsValid() && Controller->GetWorld() == World.Get())
		{
			UKismetSystemLibrary::QuitGame(World.Get(), Controller.Get(),
				static_cast<EQuitPreference::Type>(InQuitPreference), bInIgnorePlatformRestrictions);
		}
		return true;
	}
	if (bPendingUserQuit) return QuitController.Get() == InPlayer;
	bPendingUserQuit = true;
	bQuitNeedsFreshCapture = bSaveOperationRunning;
	PendingQuitPreference = InQuitPreference;
	bPendingQuitIgnorePlatformRestrictions = bInIgnorePlatformRestrictions;
	bQuitWorldWasPaused = GetWorld() && UGameplayStatics::IsGamePaused(GetWorld());
	bQuitSaveModuleWasPaused = GetModuleState() == EModuleState::Paused;
	QuitVoxel = UVoxelModule::GetPtr(GetWorld());
	bQuitVoxelModuleWasPaused = QuitVoxel.IsValid() && QuitVoxel->GetModuleState() == EModuleState::Paused;
	QuitController = InPlayer;
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
	const USettingModule* Settings = USettingModule::GetPtr(GetWorld());
	const auto Profile = !InResult ? InResult : Settings ? SaveProfile(Settings->GetActiveLocalProfile()) :
		FSaveOperationResult::Failed(ESaveResultCode::ModuleMissing, FText::FromString(TEXT("SettingModule missing.")));
	// Profile 的回调可要求停止原世界；延后停止已执行时不能再退出旧 owner。
	if (!bPendingUserQuit || !::IsValid(GetWorld())) return;
	if (!InResult || !Profile || !QuitController.IsValid() || QuitController->GetWorld() != GetWorld())
	{
		UE_LOG(LogTemp, Error, TEXT("User quit retained its live world after save failure: %s"),
			*(!InResult ? InResult.Message : !Profile ? Profile.Message : FText::FromString(TEXT("Quit controller no longer belongs to the original world"))).ToString());
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
	UWorld* World = GetWorld();
	APlayerController* Controller = QuitController.Get();
	UEventModuleStatics::BroadcastEvent<FEventGameExited>(this, { false });
	UKismetSystemLibrary::QuitGame(World, Controller,
		static_cast<EQuitPreference::Type>(PendingQuitPreference), bPendingQuitIgnorePlatformRestrictions);
}

FSaveOperationResult USaveGameModule::PollPendingSave()
{
	if (!IsBusinessAdmissionOpen()) return FSaveOperationResult::Failed(ESaveResultCode::Cancelled, FText::FromString(TEXT("原世界正在终止。")));
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

FGuid USaveGameModule::BeginPendingSaveSlot(const FCreateSaveSlotParams& Params)
{
	if (!IsBusinessAdmissionOpen() || IsTransientSession() || IsSaveOperationRunning() ||
		bPendingSlotAdoptionInProgress || bHasPendingSaveSlot) return {};
	PendingSlotSaveId.Invalidate();
	PendingSaveSlotParams = Params;
	PendingSaveSlotParams.bCaptureCurrentWorld = true;
	PendingSaveSlotToken = FGuid::NewGuid();
	bHasPendingSaveSlot = true;
	return PendingSaveSlotToken;
}

bool USaveGameModule::IsPendingSaveSlot(const FGuid& InToken) const
{
	return InToken.IsValid() && HasPendingSaveSlot() && PendingSaveSlotToken == InToken;
}

bool USaveGameModule::CancelPendingSaveSlot(const FGuid& InToken)
{
	return IsBusinessAdmissionOpen() && !PendingSlotSaveId.IsValid() && CancelPendingSaveSlotForCleanup(InToken);
}

bool USaveGameModule::CancelPendingSaveSlotForCleanup(const FGuid& InToken)
{
	if (!IsPendingSaveSlot(InToken) || IsSaveOperationRunning() || bPendingSlotAdoptionInProgress) return false;
	PendingSlotResults.Add(InToken, FSaveOperationResult::Failed(ESaveResultCode::Cancelled, FText::FromString(TEXT("原待创建存档已取消。"))));
	PendingSlotSaveId.Invalidate();
	PendingSaveSlotToken.Invalidate();
	bHasPendingSaveSlot = false;
	PendingSaveSlotParams = FCreateSaveSlotParams();
	return true;
}

FSaveOperationResult USaveGameModule::GetPendingSlotResult(const FGuid& InToken) const
{
	if (const FSaveOperationResult* Result = PendingSlotResults.Find(InToken)) return *Result;
	if (InToken.IsValid() && AsyncPendingSaveSlotToken == InToken && AsyncOperationId.IsValid())
		return FSaveOperationResult::Accepted(AsyncOperationId);
	return FSaveOperationResult::Failed(ESaveResultCode::NotFound, FText::FromString(TEXT("原待创建存档尚无完成回执。")));
}

bool USaveGameModule::ConsumePendingSlotResult(const FGuid& InToken, FSaveOperationResult& OutResult)
{
	check(IsInGameThread());
	if (!InToken.IsValid() || IsPendingSaveSlot(InToken) || AsyncPendingSaveSlotToken == InToken) return false;
	return PendingSlotResults.RemoveAndCopyValue(InToken, OutResult);
}

FSaveOperationResult USaveGameModule::SavePendingSlot(const FGuid& InToken)
{
	if (!IsBusinessAdmissionOpen() || !IsPendingSaveSlot(InToken))
		return FSaveOperationResult::Failed(ESaveResultCode::InvalidArgument, FText::FromString(TEXT("待创建存档的原始请求身份不匹配。")));
	if (bSaveOperationRunning && AsyncPendingSaveSlotToken == InToken && AsyncOperationId.IsValid())
		return FSaveOperationResult::Accepted(AsyncOperationId);
	if (IsSaveOperationRunning() || bPendingSlotAdoptionInProgress)
		return FSaveOperationResult::Failed(ESaveResultCode::Busy, FText::FromString(TEXT("存档请求正在执行。")));
	TGuardValue<bool> Adopting(bPendingSlotAdoptionInProgress, true);
	FSaveOperationResult Result;
	if (PendingSlotSaveId.IsValid()) Result = SaveSlotAsync(PendingSlotSaveId);
	else
	{
		FSaveSlotSummary Summary;
		const FCreateSaveSlotParams Params = PendingSaveSlotParams;
		Result = CreateSaveSlot(Params, Summary);
	}
	if (Result.Code == ESaveResultCode::Accepted && IsPendingSaveSlot(InToken) && AsyncOperationId == Result.OperationId)
	{
		PendingSlotResults.Remove(InToken);
		bAsyncCompletesPendingSlot = true;
		AsyncPendingSaveSlotToken = InToken;
	}
	return Result;
}

FSaveOperationResult USaveGameModule::SaveCurrentSlot()
{
	if (!IsBusinessAdmissionOpen()) return FSaveOperationResult::Failed(ESaveResultCode::Cancelled, FText::FromString(TEXT("原世界正在终止。")));
	if (IsTransientSession()) return TransientSessionUnsupported();
	if (bHasPendingSaveSlot) return FSaveOperationResult::Failed(ESaveResultCode::Busy, FText::FromString(TEXT("待创建存档必须由原请求身份提交。")));
	if (bSaveOperationRunning) return FSaveOperationResult::Accepted(AsyncOperationId);
	if (!AsyncCreatedSlotCleanup.IsCompleted())
		return FSaveOperationResult::Failed(ESaveResultCode::Busy, FText::FromString(TEXT("The original failed slot is still being retired")));
	if (ActiveSaveId.IsValid())
	{
		return SaveActiveSlot();
	}
	return FSaveOperationResult::Success();
}

void USaveGameModule::DetachActiveSave()
{
	if (!IsBusinessAdmissionOpen()) return;
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
	if (!IsBusinessAdmissionOpen()) return;
	if (IsSaveOperationRunning()) return;
	DetachActiveSave();
	if(!IsTransientSession() && Storage)
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
	if (!IsBusinessAdmissionOpen()) return Reject(ESaveResultCode::Cancelled, FText::FromString(TEXT("原世界正在终止。")));
	if (PHASEC(InPhase, EPhase::Primary) && bHasPendingSaveSlot)
		return Reject(ESaveResultCode::Busy, FText::FromString(TEXT("已有待提交世界，请先由原请求处理。")));
	FString RequiredError;
	if (!CheckRequiredWorldParticipants(RequiredError)) return Reject(ESaveResultCode::ModuleMissing, FText::FromString(RequiredError));
	if (IsTransientSession())
	{
		FSaveOperationResult Result = TransientSessionUnsupported();
		Result.OperationId = InOperationId;
		return Result;
	}
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
		const FSaveOperationResult Restored = RestoreLoadSnapshot(InPhase);
		LoadOperationResult = Restored;
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
		const FSaveOperationResult Accepted = FSaveOperationResult::Accepted(InOperationId);
		LoadOperationResult = Accepted;
		UGameplayStatics::OpenLevel(this, Manifest.CurrentMap);
		return Accepted;
	}
	LoadSnapshot = MoveTemp(Snapshot);
	const FSaveOperationResult Restored = RestoreLoadSnapshot(InPhase);
	LoadOperationResult = Restored;
	LoadOperationResult.OperationId = InOperationId;
	return LoadOperationResult;
}

FSaveOperationResult USaveGameModule::RestoreLoadSnapshot(EPhase InPhase)
{
	FString RequiredError;
	if (!CheckRequiredWorldParticipants(RequiredError))
	{
		LoadSnapshot = FSaveGameLoadSnapshot();
		return FSaveOperationResult::Failed(ESaveResultCode::ModuleMissing, FText::FromString(RequiredError));
	}
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
	TGuardValue<bool> Loading(bApplyingWorldLoad, true);
	TGuardValue<bool> Applying(bApplyingSaveCallbacks, true);
	FSaveOperationResult Result = LoadModulesFromGeneration(Context.SaveId, Context.Generation, InPhase);
	if (LoadSnapshot.Context.OperationId != Context.OperationId)
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
	}
	if (!Result || PHASEC(InPhase, EPhase::Final))
	{
		LoadSnapshot = FSaveGameLoadSnapshot();
	}
	return Result;
}

FSaveOperationResult USaveGameModule::LoadModulesFromGeneration(const FGuid& SaveId, int32 Generation, EPhase InPhase)
{
	FString RequiredError;
	if (!CheckRequiredWorldParticipants(RequiredError)) return FSaveOperationResult::Failed(ESaveResultCode::ModuleMissing, FText::FromString(RequiredError));
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
	const TArray<UModuleBase*> SourceModules = GetSaveModules(ESaveScope::World, false);
	for (UModuleBase* Module : SourceModules)
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
    TMap<FName, FParameter> GuardModules;
    for(const FLoadedModuleSaveData& Item : Loaded) GuardModules.Add(Item.Module->GetModuleName(), Item.Data);
    FString GuardError;
    const bool bBeforeGuardsPassed = RunWorldLoadGuards(SaveId, GuardModules, InPhase, false, GuardError);
    if(!bBeforeGuardsPassed)
    {
        return FSaveOperationResult::Failed(ESaveResultCode::LoadFailed, FText::FromString(GuardError));
    }
	const auto LoadFailed = [&Loaded](const FLoadedModuleSaveData& FailedItem, EPhase FailedPhase)
	{
		const FName ModuleName = FailedItem.Header.ModuleName;
		for (FLoadedModuleSaveData& Item : Loaded)
		{
			if (::IsValid(Item.Module)) Item.Module->OnAfterLoadData(false);
		}
		return FSaveOperationResult::Failed(ESaveResultCode::LoadFailed,
			FText::FromString(FString::Printf(TEXT("模块 %s 的存档加载阶段 %d 失败。"), *ModuleName.ToString(), static_cast<int32>(FailedPhase))));
	};
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
			USceneModule* Scene = Cast<USceneModule>(Item.Module);
			const bool bLoaded = Scene ? Scene->LoadSceneSaveData(Item.Data, EPhase::Primary) : Item.Module->LoadSaveData(Item.Data, EPhase::Primary);
			if (!bLoaded) return LoadFailed(Item, EPhase::Primary);
		}
	}
	if (PHASEC(InPhase, EPhase::Lesser))
	{
		for (FLoadedModuleSaveData& Item : Loaded)
		{
			USceneModule* Scene = Cast<USceneModule>(Item.Module);
			const bool bLoaded = Scene ? Scene->LoadSceneSaveData(Item.Data, EPhase::Lesser) : Item.Module->LoadSaveData(Item.Data, EPhase::Lesser);
			if (!bLoaded) return LoadFailed(Item, EPhase::Lesser);
		}
	}
	if (PHASEC(InPhase, EPhase::Final))
	{
		for (FLoadedModuleSaveData& Item : Loaded)
		{
			USceneModule* Scene = Cast<USceneModule>(Item.Module);
			const bool bLoaded = Scene ? Scene->LoadSceneSaveData(Item.Data, EPhase::Final) : Item.Module->LoadSaveData(Item.Data, EPhase::Final);
			if (!bLoaded) return LoadFailed(Item, EPhase::Final);
		}
	}
    const bool bAfterGuardsPassed = RunWorldLoadGuards(SaveId, GuardModules, InPhase, true, GuardError);
    if(!bAfterGuardsPassed)
    {
        if(PHASEC(InPhase, EPhase::Final))
        {
            for(FLoadedModuleSaveData& Item : Loaded)
			{
				Item.Module->OnAfterLoadData(false);
			}
        }
        return FSaveOperationResult::Failed(ESaveResultCode::LoadFailed, FText::FromString(GuardError));
    }
	if(PHASEC(InPhase, EPhase::Final))
	{
		for(FLoadedModuleSaveData& Item : Loaded)
		{
			Item.Module->OnAfterLoadData(true);
		}
	}
	return FSaveOperationResult::Success();
}

FSaveOperationResult USaveGameModule::DeleteSaveSlot(FGuid SaveId)
{
	if (!IsBusinessAdmissionOpen()) return FSaveOperationResult::Failed(ESaveResultCode::Cancelled, FText::FromString(TEXT("原世界正在终止。")));
	if (IsTransientSession()) return TransientSessionUnsupported();
	FString RequiredError;
	if (!CheckRequiredWorldParticipants(RequiredError)) return FSaveOperationResult::Failed(ESaveResultCode::ModuleMissing, FText::FromString(RequiredError));
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
	if (!CheckRequiredWorldParticipants(RequiredError)) return FSaveOperationResult::Failed(ESaveResultCode::ModuleMissing, FText::FromString(RequiredError));
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
	if (!IsBusinessAdmissionOpen()) return FSaveOperationResult::Failed(ESaveResultCode::Cancelled, FText::FromString(TEXT("原世界正在终止。")));
	FString RequiredError;
	if (!CheckRequiredWorldParticipants(RequiredError)) return FSaveOperationResult::Failed(ESaveResultCode::ModuleMissing, FText::FromString(RequiredError));
	if (IsTransientSession()) return TransientSessionUnsupported();
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
	if (!IsBusinessAdmissionOpen()) return FSaveOperationResult::Failed(ESaveResultCode::Cancelled, FText::FromString(TEXT("原世界正在终止。")));
	if (IsTransientSession()) return TransientSessionUnsupported();
	FString RequiredError;
	if (!CheckRequiredWorldParticipants(RequiredError)) return FSaveOperationResult::Failed(ESaveResultCode::ModuleMissing, FText::FromString(RequiredError));
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
	bool bParticipantFinished = true;
    auto Finish = [&]()
    {
        if (bFinished) return;
        bFinished = true;
        FString FinishError;
        if (External)
        {
			bParticipantFinished =
				FinishExternalStore(Participant, External, bPublished, FinishError);
            External->Store->CloseNative();
            const bool bCleaned = (!bParticipantFinished || External->Context.bPublicationUncertain) ? false :
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
		if (External)
		{
			const bool bTransformed = Participant.Transform(Context, External->Store, SourceModuleName, Data, Error);
			if (!bTransformed) return Fail(ESaveResultCode::SerializeFailed, Error);
		}
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
	if (!bParticipantFinished)
		return Fail(ESaveResultCode::ConfirmationPending, TEXT("Duplicate files were published but the original World participant did not finish; recovery intent was retained."));
    FSaveOperationResult Result = FSaveOperationResult::Success();
    Result.OperationId = Context.OperationId;
    return Result;
}

FSaveSlotSummary USaveGameModule::GetSaveSlotSummary(FGuid SaveId) const
{
	if (IsTransientSession()) return FSaveSlotSummary();
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
	if (IsTransientSession()) return Result;
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
	if (!IsValidProfileName(ProfileName)) return FSaveOperationResult::Failed(ESaveResultCode::InvalidArgument, FText::FromString(TEXT("Invalid profile name.")));
	if (!IsBusinessAdmissionOpen() || bHostTerminationRequested) return FSaveOperationResult::Failed(ESaveResultCode::Cancelled, FText::FromString(TEXT("Original World is terminating")));
	if (IsTransientSession()) return TransientSessionUnsupported();
	if (IsSaveOperationRunning())
	{
		return FSaveOperationResult::Failed(ESaveResultCode::Busy, FText::FromString(TEXT("Save transaction is in progress")));
	}
	USettingModule* Setting = USettingModule::GetPtr(GetWorld());
	if (!Setting || !Storage)
	{
		return FSaveOperationResult::Failed(ESaveResultCode::ModuleMissing, FText::FromString(TEXT("SettingModule missing.")));
	}
	if (!Setting->CanSaveLocalProfile(ProfileName))
	{
		return FSaveOperationResult::Failed(ESaveResultCode::InvalidArgument, FText::FromString(TEXT("Profile does not match the loaded local settings identity.")));
	}
	FSaveOperationResult Result;
	{
		TGuardValue<bool> Applying(bApplyingSaveCallbacks, true);
		Setting->OnBeforeSaveData();
		TArray<uint8> Bytes;
		const bool bBuilt = Setting->BuildSaveFile(Bytes);
		const bool bSaved = bBuilt &&
			Storage->WriteBinaryAtomic(Storage->GetProfileModuleFilePath(ProfileName, Setting->GetModuleName()), Bytes);
		Setting->OnAfterSaveData(bSaved);
		Result = bSaved ? FSaveOperationResult::Success() :
			FSaveOperationResult::Failed(bBuilt ? ESaveResultCode::WriteFailed : ESaveResultCode::CaptureFailed,
				FText::FromString(bBuilt ? TEXT("Profile file could not be written.") : TEXT("Profile settings could not be captured or serialized.")));
	}
	return Result;
}

FSaveOperationResult USaveGameModule::LoadProfile(FName ProfileName)
{
	if (!IsValidProfileName(ProfileName)) return FSaveOperationResult::Failed(ESaveResultCode::InvalidArgument, FText::FromString(TEXT("Invalid profile name.")));
	if (!IsBusinessAdmissionOpen() || bHostTerminationRequested) return FSaveOperationResult::Failed(ESaveResultCode::Cancelled, FText::FromString(TEXT("Original World is terminating")));
	if (IsTransientSession()) return TransientSessionUnsupported();
	if (IsSaveOperationRunning())
	{
		return FSaveOperationResult::Failed(ESaveResultCode::Busy, FText::FromString(TEXT("Save transaction is in progress")));
	}
	USettingModule* Setting = USettingModule::GetPtr(GetWorld());
	TArray<uint8> Bytes;
	FModuleSaveFileHeader Header;
	FParameter Data;
	if (!Setting || !Storage)
	{
		return FSaveOperationResult::Failed(ESaveResultCode::ModuleMissing, FText::FromString(TEXT("Profile storage or SettingModule is unavailable.")));
	}
	const FString ProfilePath = Storage->GetProfileModuleFilePath(ProfileName, Setting->GetModuleName());
	if (!Storage->ReadBinary(ProfilePath, Bytes))
	{
		const bool bExists = IFileManager::Get().FileExists(*ProfilePath);
		return FSaveOperationResult::Failed(bExists ? ESaveResultCode::ReadFailed : ESaveResultCode::NotFound,
			FText::FromString(bExists ? TEXT("Profile could not be read.") : TEXT("Profile not found.")));
	}
	if (!FSaveDataSerializer::ReadModuleFile(Bytes, Header, Data) || Header.ModuleName != Setting->GetModuleName())
	{
		return FSaveOperationResult::Failed(ESaveResultCode::CorruptData, FText::FromString(TEXT("Profile data is corrupt.")));
	}
	FSaveOperationResult Result;
	{
		TGuardValue<bool> Applying(bApplyingSaveCallbacks, true);
		const bool bMigrated = Header.ModuleVersion == Setting->GetSaveDataVersion() || Setting->MigrateSaveData(Header.ModuleVersion, Data);
		if (!bMigrated)
		{
			Result = FSaveOperationResult::Failed(ESaveResultCode::VersionMismatch, FText::FromString(TEXT("Profile version mismatch.")));
		}
		else if (!Setting->IsLocalProfileDataSupported(Data))
		{
			Result = FSaveOperationResult::Failed(ESaveResultCode::CorruptData, FText::FromString(TEXT("Profile contains invalid settings data.")));
		}
		else
		{
			const bool bPrepared = Setting->PrepareLocalProfileLoad(ProfileName);
			if (!bPrepared)
			{
				if (!Setting->CanSaveLocalProfile(Setting->GetActiveLocalProfile())) Setting->AbortLocalProfileLoad();
				Result = FSaveOperationResult::Failed(ESaveResultCode::LoadFailed, FText::FromString(TEXT("Profile could not prepare the local setting modules.")));
			}
			else
			{
				Setting->OnBeforeLoadData();
				const bool bPrimaryLoaded = Setting->LoadLocalProfilePhase(Data, EPhase::Primary);
				const bool bLoaded = bPrimaryLoaded && Setting->LoadLocalProfilePhase(Data, EPhase::Final);
				if (!bLoaded) Setting->AbortLocalProfileLoad();
				Setting->OnAfterLoadData(bLoaded);
				const bool bStillLoaded = bLoaded && Setting->CanSaveLocalProfile(ProfileName);
				if (!bStillLoaded && Setting->GetActiveLocalProfile() == ProfileName) Setting->AbortLocalProfileLoad();
				Result = bStillLoaded ? FSaveOperationResult::Success() :
					FSaveOperationResult::Failed(ESaveResultCode::LoadFailed, FText::FromString(TEXT("Profile settings were not fully applied.")));
			}
		}
	}
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
	if(IsTransientSession() || InEvent.bIsSimulating || bQuitSaveCommitted)
	{
		return;
	}

	const FSaveOperationResult SaveResult = SaveCurrentSlot();
	if(!SaveResult)
	{
		UE_LOG(LogTemp, Error, TEXT("Saving before exit failed: %s"), *SaveResult.Message.ToString());
	}
	const USettingModule* Settings = USettingModule::GetPtr(GetWorld());
	const FSaveOperationResult ProfileResult = Settings ? SaveProfile(Settings->GetActiveLocalProfile()) :
		FSaveOperationResult::Failed(ESaveResultCode::ModuleMissing, FText::FromString(TEXT("SettingModule missing.")));
	if (!ProfileResult)
	{
		UE_LOG(LogTemp, Error, TEXT("Saving the local profile before exit failed: %s"), *ProfileResult.Message.ToString());
	}
}


void USaveGameModule::RequireWorldStoreParticipant(FName InName)
{
	check(!InName.IsNone() && (RequiredWorldStoreParticipant.IsNone() || RequiredWorldStoreParticipant == InName));
	RequiredWorldStoreParticipant = InName;
}

void USaveGameModule::RequireWorldLoadGuard(FName InName)
{
	check(!InName.IsNone());
	RequiredWorldLoadGuards.Add(InName);
}

bool USaveGameModule::CheckRequiredWorldParticipants(FString& OutError) const
{
	if (!IsBusinessAdmissionOpen() || bHostTerminationRequested)
	{
		OutError = TEXT("Original World is terminating");
		return false;
	}
	if (!RequiredWorldStoreParticipant.IsNone() &&
		(WorldStoreParticipantName != RequiredWorldStoreParticipant || !WorldStoreParticipant.IsAvailable || !WorldStoreParticipant.IsAvailable() || !WorldStoreParticipant.Begin ||
		 !WorldStoreParticipant.Transform || !WorldStoreParticipant.Finish || !WorldStoreParticipant.BeforeDelete || !WorldStoreParticipant.Abandon))
	{
		OutError = FString::Printf(TEXT("Required world store participant is unavailable: %s"), *RequiredWorldStoreParticipant.ToString());
		return false;
	}
	for (FName Name : RequiredWorldLoadGuards)
	{
		const FWorldLoadGuardEntry* Guard = WorldLoadGuards.Find(Name);
		if (!Guard || !Guard->Before || !Guard->After)
		{
			OutError = FString::Printf(TEXT("Required world load guard is unavailable: %s"), *Name.ToString());
			return false;
		}
	}
	return true;
}

bool USaveGameModule::RegisterWorldLoadGuard(FName InName, FSaveWorldLoadGuard InBefore, FSaveWorldLoadGuard InAfter)
{
	if (!IsBusinessAdmissionOpen()) return false;
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
    if (!CheckRequiredWorldParticipants(OutError)) return false;
    TArray<FName> Names;
    WorldLoadGuards.GetKeys(Names);
    Names.Sort(FNameLexicalLess());
    for(FName Name : Names)
    {
        FWorldLoadGuardEntry* Entry = WorldLoadGuards.Find(Name);
        if(!Entry) continue;
        FSaveWorldLoadGuard Guard = bInAfter ? Entry->After : Entry->Before;
		const bool bPassed = Guard(InSaveId, InModules, InPhase, OutError);
        if(!bPassed)
        {
            if(OutError.IsEmpty()) OutError = FString::Printf(TEXT("World load guard failed: %s"), *Name.ToString());
            return false;
        }
		if (!CheckRequiredWorldParticipants(OutError)) return false;
    }
    return true;
}

bool USaveGameModule::RegisterWorldStoreParticipant(FName InName, FSaveWorldStoreParticipant InParticipant)
{
	if (!IsBusinessAdmissionOpen()) return false;
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
    return !IsTransientSession() && Storage ? Storage->GetRootDir() : FString();
}

bool USaveGameModule::IsStepTimeLimitEnforced()
{
	return CVarSaveStepTimeLimit.GetValueOnGameThread() != 0;
}

FString USaveGameModule::GetUnboundInventoryRoot() const
{
	if (IsTransientSession()) return FString();
	return LocalSaveContext ? LocalSaveContext->GetUnboundInventoryRoot() :
		FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Inventory"), TEXT("Unbound"));
}

bool USaveGameModule::BeginExternalStore(FSaveWorldCaptureContext InContext,
    const TMap<FName, FParameter>& InSourceModules, FSaveWorldStoreParticipant& OutParticipant,
    TSharedPtr<FSaveWorldExternalOperation, ESPMode::ThreadSafe>& OutOperation, FString& OutError)
{
    check(IsInGameThread());
	if (!CheckRequiredWorldParticipants(OutError)) return false;
    OutOperation.Reset();
    OutParticipant = WorldStoreParticipant;
    if (!OutParticipant.Begin) return true;
    TGuardValue<bool> Applying(bApplyingSaveCallbacks, true);
    TSharedPtr<ISaveWorldExternalStore, ESPMode::ThreadSafe> Prepared;
    const bool bBegan = OutParticipant.Begin(InContext, InSourceModules, Prepared, OutError);
	if (!bBegan) return false;
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

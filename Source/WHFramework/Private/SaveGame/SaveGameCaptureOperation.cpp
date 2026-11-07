#include "SaveGame/SaveGameCaptureOperation.h"
#include "SaveGame/SaveOperationTypes.h"
#include "Common/LifecycleTypes.h"
#include "SaveGame/SaveGameModule.h"

#include "GameFramework/PlayerController.h"
#include "Main/MainModule.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "SaveGame/SaveDataSerializer.h"
#include "SaveGame/SaveGameAsyncExecutor.h"
#include "SaveGame/SaveGameStorage.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/Task/VoxelTerrainResources.h"

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
		if (!AsyncModuleData.HasValue())
		{
			OutError = FString::Printf(TEXT("Module %s returned no captured save payload"), *Module->GetModuleName().ToString());
			return false;
		}
        if (AsyncExternalOperation && AsyncStoreParticipant.Transform)
		{
			const bool bTransformed = AsyncStoreParticipant.Transform(AsyncExternalOperation->Context, AsyncExternalOperation->Store,
				Module->GetModuleName(), AsyncModuleData, OutError);
			if (!bTransformed) return false;
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
	if (!AsyncCompletionResult.IsSet() || (bApplyingSaveCallbacks && !bForce)) return;
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
			AsyncCreatedSlotCleanup = UE::Tasks::Launch(UE_SOURCE_LOCATION, FCreatedSlotCleanupTask{AsyncPlan},
				UE::Tasks::ETaskPriority::BackgroundNormal, UE::Tasks::EExtendedTaskPriority::None,
				UE::Tasks::ETaskFlags::DoNotRunInsideBusyWait);
			FSaveGameAsyncExecutor::TrackNativeRetirement(AsyncCreatedSlotCleanup, AsyncPlan->MetadataLease);
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
}

void USaveGameModule::FinalizeAsyncSave()
{
	check(IsInGameThread() && AsyncCompletionResult.IsSet() && AsyncCompletionCursor == AsyncPrepareCount);
    if (AsyncExternalOperation)
    {
        FString Error;
        TGuardValue<bool> Applying(bApplyingSaveCallbacks, true);
        const bool bFinished = FinishExternalStore(AsyncStoreParticipant, AsyncExternalOperation, bAsyncCompletionFilesCommitted, Error);
        if (!bFinished)
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
	const FGuid CompletedPendingToken = AsyncPendingSaveSlotToken;
	const bool bOwnsPendingSlot = bAsyncCompletesPendingSlot && IsPendingSaveSlot(CompletedPendingToken);
	const bool bFinishedPendingSlot = Completed && bOwnsPendingSlot;
	if (!Completed && bFilesCommitted && bOwnsPendingSlot) PendingSlotSaveId = CompletedId;
	AsyncPendingSaveSlotToken.Invalidate();
	bAsyncCompletesPendingSlot = false;
	bAsyncWorkerStarted = false;
	bAsyncWorkerCapacityBlocked = false;
	AsyncWorld = nullptr;
	bSaveOperationRunning = false;
	if (bFinishedPendingSlot) CancelPendingSaveSlotForCleanup(CompletedPendingToken);
	if (bOwnsPendingSlot) PendingSlotResults.Add(CompletedPendingToken, Completed);
	LastWorldSaveResult = Completed;
	{
		TGuardValue<bool> Applying(bApplyingSaveCallbacks, true);
		OnWorldSaveFinished.Broadcast(CompletedId, Completed);
	}
	if (bPendingUserQuit) CompleteUserQuit(Completed);
}

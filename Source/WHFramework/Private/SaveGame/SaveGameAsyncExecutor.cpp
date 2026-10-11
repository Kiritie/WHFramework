#include "SaveGame/SaveGameAsyncExecutor.h"
#include "SaveGame/SaveGameStorage.h"
#include "Misc/Paths.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/Rendering/VoxelTerrainFrameContext.h"
#include "Misc/ScopeExit.h"

struct FSaveGameAsyncExecutor::FWorker
{
	TSharedPtr<FState, ESPMode::ThreadSafe> State;
	void operator()() const
	{
		const auto& Plan = State->Plan;
		State->AttemptReleaseVersion = State->Resources ? State->Resources->ReleaseVersionExcludingOwner(State->Consumer) : 0;
		State->AttemptWriterReleaseVersion = FSaveGameGenerationWriteLease::ReleaseVersion();
		FSaveGameStorage Storage(Plan.UserIndex, Plan.StorageRoot, Plan.TransactionId);
		const ESaveWorldWriteResult WriteResult = Storage.BeginWorldWrite(Plan.SaveId, State->Resources, State->Consumer);
		if (WriteResult != ESaveWorldWriteResult::Acquired)
		{
			if (State->Cancel.load())
			{
				State->Result = FSaveOperationResult::Failed(ESaveResultCode::Busy,
					FText::FromString(TEXT("Save canceled before acquiring write ownership; the slot directory was retained")));
			}
			else
			{
				State->Result = FSaveOperationResult::Failed(WriteResult == ESaveWorldWriteResult::CapacityBlocked ?
					ESaveResultCode::CapacityBlocked : WriteResult == ESaveWorldWriteResult::Busy ? ESaveResultCode::SlotBusy :
					ESaveResultCode::InvalidArgument, FText::FromString(TEXT("Original save transaction is waiting to own its slot")));
			}
			return;
		}
		State->Result = Execute(Plan, State->Cancel, Storage);
		if (!State->Result && State->Result.Code != ESaveResultCode::CapacityBlocked && Plan.Voxel.IsSet() &&
			Plan.Voxel->Voxels.Completion && !Plan.Voxel->Voxels.Completion->bGenerationCommitted)
			Plan.Voxel->Voxels.Completion->TargetReadLease.Reset();
        const bool bTerminal = State->Result.Code != ESaveResultCode::CapacityBlocked &&
            State->Result.Code != ESaveResultCode::SlotBusy;
        if (bTerminal && Plan.ExternalOperation)
        {
            FString Error;
            if (Plan.ExternalOperation->bFilesCommitted.load())
            {
                // Manifest 已发布，即使清除 intent 失败也保留目标并让 GT 采用同一准备句柄。
                if (!Storage.CompletePendingWorld(Plan.ExternalOperation->PendingLease, Error))
                    UE_LOG(LogTemp, Warning, TEXT("Committed world branch retains its recovery intent: %s"), *Error);
            }
            else if (Plan.ExternalOperation->Context.bPublicationUncertain)
            {
                Plan.ExternalOperation->Store->CloseNative();
                UE_LOG(LogTemp, Error, TEXT("目标 Manifest 暂时不可读，保留整个分支与 intent 等待确认。"));
            }
            else
            {
                Plan.ExternalOperation->Store->CloseNative();
                if (!Storage.AbortPendingWorld(Plan.ExternalOperation->PendingLease, Error))
                    State->Result.Message = FText::FromString(State->Result.Message.ToString() + TEXT("; ") + Error);
            }
        }
        else if (bTerminal && !State->Result && Plan.bDeleteCreatedSlotOnFailure)
        {
            FSaveManifest Current;
            if (Storage.ReadManifest(Plan.SaveId, Current) && Current.CurrentGeneration == 0 &&
                Plan.Manifest.CurrentGeneration == 0 && !Storage.DeleteWorldDirectory(Plan.SaveId))
                State->Result.Message = FText::FromString(TEXT("保存失败，未提交的新槽暂时保留，待持有者退出后回收。"));
        }
	}
};

struct FSaveGameAsyncExecutor::FReclaimer
{
	TSharedPtr<FState, ESPMode::ThreadSafe> State;
	UE::Tasks::FTask NativeTask;
	void operator()() const
	{
		check(NativeTask.IsCompleted());
		// 原生任务真正退出后才释放持有它的控制根，且不在 GT 等待。
	}
};

FSaveGameAsyncExecutor::~FSaveGameAsyncExecutor()
{
	if (State)
	{
		State->Cancel.store(true);
		if (Task.IsCompleted() && (State->Result.Code == ESaveResultCode::CapacityBlocked ||
			State->Result.Code == ESaveResultCode::SlotBusy))
		{
			Task = {};
			Task = UE::Tasks::Launch(UE_SOURCE_LOCATION, FWorker{State});
		}
		const UE::Tasks::FTask NativeTask = Task;
		UE::Tasks::Launch(UE_SOURCE_LOCATION, FReclaimer{MoveTemp(State), MoveTemp(Task)},
			UE::Tasks::Prerequisites(NativeTask));
	}
}
bool FSaveGameAsyncExecutor::Start(FSaveGenerationPlan&& P, FString* OutError)
{
	check(IsInGameThread());
	if (State || !P.SaveId.IsValid())
	{
		if (OutError) *OutError = TEXT("Save worker already owns a live transaction");
		return false;
	}
	if (!P.TransactionId.IsValid())
		P.TransactionId = P.Voxel.IsSet() ? P.Voxel->Voxels.TransactionId : FGuid::NewGuid();
	if (!P.TransactionId.IsValid() || (P.Voxel.IsSet() && P.TransactionId != P.Voxel->Voxels.TransactionId))
	{
		if (OutError) *OutError = TEXT("Save worker transaction differs from its frozen voxel cut");
		return false;
	}
	if (!P.OperationId.IsValid()) P.OperationId = P.TransactionId;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	if (P.Voxel.IsSet() && P.Voxel->Voxels.Journal.Resources)
	{
		const auto& Capture = P.Voxel->Voxels;
		const uint64 Bytes = VoxelBoundedAllocation::WorkerSharedBytes<FState>() +
			VoxelBoundedAllocation::TaskBytes<FWorker>() + VoxelBoundedAllocation::TaskBytes<FReclaimer>();
		if (!Capture.Journal.Resources->Acquire(FVoxelGenerationConsumerKey::Section(Capture.WorldEpoch,
			Capture.TransactionId, 1, {}, 6), EVoxelTerrainResourcePool::Input, Bytes, Lease))
		{
			if (OutError) *OutError = TEXT("CapacityBlocked: save native worker and completion reclaimer were not granted");
			return false;
		}
	}
	State = Lease ? VoxelBoundedAllocation::MakeWorkerShared<FState>(Lease) :
		TSharedPtr<FState, ESPMode::ThreadSafe>(new FState(), VoxelBoundedAllocation::TWorkerDeleter<FState>{nullptr});
	State->MetadataLease = MoveTemp(Lease);
	if (P.Voxel.IsSet())
	{
		State->Resources = P.Voxel->Voxels.Journal.Resources;
		State->Consumer = FVoxelGenerationConsumerKey::Section(P.Voxel->Voxels.WorldEpoch, P.Voxel->Voxels.TransactionId, 1, {}, 6);
	}
	State->Plan = MoveTemp(P);
	Task = UE::Tasks::Launch(UE_SOURCE_LOCATION, FWorker{State});
	if (OutError) OutError->Reset();
	return true;
}
bool FSaveGameAsyncExecutor::Poll(FSaveOperationResult& O, FVoxelTerrainFrameContext* InFrame, const bool bForceNativeControl)
{
	check(IsInGameThread());
	if (!State || !Task.IsCompleted())
		return false;
	const bool bSlotRetry = State->Result.Code == ESaveResultCode::SlotBusy;
	const bool bCapacityRetry = State->Result.Code == ESaveResultCode::CapacityBlocked;
	if (bSlotRetry && !State->Cancel.load() &&
		FSaveGameGenerationWriteLease::ReleaseVersion() <= State->AttemptWriterReleaseVersion) return false;
	if (bCapacityRetry)
	{
		uint64 ReleaseVersion = 0;
		if (!State->Cancel.load() && (!State->Resources ||
			!State->Resources->TryGetReleaseVersionExcludingOwner(State->Consumer, ReleaseVersion) ||
			ReleaseVersion <= State->AttemptReleaseVersion)) return false;
	}
	if (!bForceNativeControl && State->Resources && (!InFrame || !InFrame->IsActive() ||
		InFrame->GetWorldEpoch() != State->Consumer.WorldEpoch)) return false;
	const bool bRetry = bSlotRetry || bCapacityRetry;
	const EVoxelPublishPhase Phase = bRetry ? EVoxelPublishPhase::Prepare : EVoxelPublishPhase::Retire;
	double& CostMilliseconds = bRetry ? RetryCostMilliseconds : CompletionCostMilliseconds;
	const double Started = FPlatformTime::Seconds();
	const bool bBudgeted = !bForceNativeControl && InFrame && InFrame->IsActive();
	if (bBudgeted && !InFrame->GetPublishBudget().TryStart(Phase, Started, CostMilliseconds))
	{
		if (!InFrame->IsOverdueOpportunityAvailable()) return false;
		InFrame->ConsumeOverdueOpportunity();
	}
	ON_SCOPE_EXIT
	{
		const double Finished = FPlatformTime::Seconds();
		CostMilliseconds = FMath::Lerp(CostMilliseconds, (Finished - Started) * 1000.0, 0.15);
		if (bBudgeted) InFrame->GetPublishBudget().Finish(Phase, Started, Finished);
	};
	if (bRetry)
	{
		Task = {};
		Task = UE::Tasks::Launch(UE_SOURCE_LOCATION, FWorker{State});
		return false;
	}
	O = State->Result;
	Task = UE::Tasks::FTask();
	State.Reset();
	return true;
}
void FSaveGameAsyncExecutor::RequestCancel()
{
	if (State)
		State->Cancel.store(true);
}

bool FSaveGameAsyncExecutor::IsWaitingForCapacity() const
{
	check(IsInGameThread());
	return State && Task.IsCompleted() && State->Result.Code == ESaveResultCode::CapacityBlocked;
}

bool FSaveGameAsyncExecutor::IsWaitingForSlot() const
{
	check(IsInGameThread());
	return State && Task.IsCompleted() && State->Result.Code == ESaveResultCode::SlotBusy;
}

FSaveOperationResult FSaveGameAsyncExecutor::Execute(const FSaveGenerationPlan& P, const std::atomic_bool& Cancel,
	FSaveGameStorage& Storage)
{
	auto Fail = [](ESaveResultCode C, const TCHAR* T)
	{
		return FSaveOperationResult::Failed(C, FText::FromString(T));
	};
	if (Cancel.load()) return Fail(ESaveResultCode::Busy, TEXT("Save canceled before write"));
    const TSharedPtr<FSavePendingWorldLease, ESPMode::ThreadSafe> PendingLease = P.ExternalOperation ?
        P.ExternalOperation->PendingLease : TSharedPtr<FSavePendingWorldLease, ESPMode::ThreadSafe>();
    FSaveManifest Current;
    if (!Storage.ReadManifest(P.SaveId, Current))
    {
        if (!P.bCreateSlotAtWrite || !Storage.CreateWorldDirectory(P.SaveId) ||
            !Storage.WriteManifestAtomic(P.SaveId, P.Manifest, PendingLease))
            return Fail(ESaveResultCode::WriteFailed, TEXT("无法建立本次操作持有的新存档槽。"));
        Current = P.Manifest;
    }
    if ((!P.bResolveManifestOnWorker && Current.CurrentGeneration != P.Manifest.CurrentGeneration) ||
        (P.ExternalOperation && (Current.CurrentGeneration != 0 ||
            P.ExternalOperation->Context.TargetGeneration != 1)))
        return Fail(ESaveResultCode::CommitFailed, TEXT("目标存档代次已变化，不能覆盖另一个已提交世界。"));
	if (Cancel.load())
		return Fail(ESaveResultCode::Busy, TEXT("Save canceled before write"));
	if (Current.CurrentGeneration == MAX_int32)
		return Fail(ESaveResultCode::CommitFailed, TEXT("Generation counter exhausted"));
	const int32 Next = Current.CurrentGeneration + 1;
	if (P.Voxel.IsSet())
	{
		const auto& Capture = P.Voxel->Voxels;
		const auto& Completion = Capture.Completion;
		if (Completion && Completion->TargetGeneration > 0 &&
			(Completion->TargetSaveId != P.SaveId || Completion->TargetGeneration != Next))
			return Fail(ESaveResultCode::CommitFailed, TEXT("Accepted save target generation changed while waiting for capacity"));
		if (Completion && !Completion->TargetDirectory)
		{
			if (!Completion->ExpectedStorageRoot || Completion->ExpectedSaveId != P.SaveId ||
				!FPaths::IsSamePath(*Completion->ExpectedStorageRoot, Storage.GetRootDir()) || !Capture.Journal.Resources)
				return Fail(ESaveResultCode::CaptureFailed, TEXT("Save worker has no matching accepted slot/root owner"));
			Completion->TargetSaveId = P.SaveId;
			Completion->TargetGeneration = Next;
			const FString Directory = Storage.GetGenerationDir(P.SaveId, Next);
			const uint64 Bytes = VoxelBoundedAllocation::SharedBytes<FString>() +
				2 * VoxelBoundedAllocation::ArrayBytes<TCHAR>(Directory.Len() + 1);
			if (!Capture.Journal.Resources->Acquire(FVoxelGenerationConsumerKey::Section(Capture.WorldEpoch,
				Capture.TransactionId, 1, {}, 8), EVoxelTerrainResourcePool::Input, Bytes, Completion->TargetLease))
				return Fail(ESaveResultCode::CapacityBlocked, TEXT("CapacityBlocked: actual save generation target was not granted"));
			Completion->TargetDirectory = MakeShared<const FString, ESPMode::ThreadSafe>(Directory);
		}
		if (!Completion || Completion->TransactionId != Capture.TransactionId ||
			Completion->WorldEpoch != Capture.WorldEpoch || Completion->TargetSaveId != P.SaveId ||
			Completion->TargetGeneration != Next || !Completion->TargetDirectory ||
			!FPaths::IsSamePath(*Completion->TargetDirectory, Storage.GetGenerationDir(P.SaveId, Next)))
		{
			return Fail(ESaveResultCode::CaptureFailed, TEXT("Voxel save target does not match its captured transaction"));
		}
		Completion->bSceneFilesWritten = false;
		Completion->bFilesFlushed = false;
		Completion->bGenerationCommitted = false;
	}
	if (!Storage.CleanupUncommittedGenerations(P.SaveId, Current.CurrentGeneration))
		return Fail(ESaveResultCode::WriteFailed, TEXT("Original save transaction could not retire uncommitted generations"));
	const FString* SourceGeneration = P.Voxel.IsSet() ? P.Voxel->Voxels.SourceDirectory.Get() : nullptr;
	if (SourceGeneration && !SourceGeneration->IsEmpty() &&
		(!P.Voxel->Voxels.SourceLease || !P.Voxel->Voxels.SourceLease->Matches(*SourceGeneration)))
		return Fail(ESaveResultCode::CaptureFailed, TEXT("Save worker lost its captured voxel baseline generation lease"));
	const auto* SourceLease = P.Voxel.IsSet() ? &P.Voxel->Voxels.SourceLease : nullptr;
	if (!Storage.PrepareTempGeneration(P.SaveId, Current.CurrentGeneration, Next, SourceGeneration, SourceLease))
		return Fail(ESaveResultCode::WriteFailed, TEXT("Prepare generation failed"));
	for (const auto& M : P.Modules)
	{
		if (Cancel.load())
			return Fail(ESaveResultCode::Busy, TEXT("Save canceled before commit"));
		if (!Storage.WriteBinary(Storage.GetTempModuleFilePath(P.SaveId, Next, M.ModuleName), M.Bytes))
			return Fail(ESaveResultCode::WriteFailed, TEXT("Module file write failed"));
	}
	if (P.Voxel.IsSet())
	{
		FString E;
		if (!UVoxelModule::WriteSaveCapture(P.Voxel.GetValue(), Storage.GetTempGenerationDir(P.SaveId, Next), E))
			return FSaveOperationResult::Failed(E.StartsWith(TEXT("CapacityBlocked:")) ?
				ESaveResultCode::CapacityBlocked : ESaveResultCode::WriteFailed, FText::FromString(E));
	}
	if (Cancel.load())
		return Fail(ESaveResultCode::Busy, TEXT("Save canceled before commit"));
	if (!Storage.FlushTempGeneration(P.SaveId, Next))
		return Fail(ESaveResultCode::WriteFailed, TEXT("Generation file flush failed"));
	if (P.Voxel.IsSet()) P.Voxel->Voxels.Completion->bFilesFlushed = true;
	if (Cancel.load())
		return Fail(ESaveResultCode::Busy, TEXT("Save canceled after file flush"));
	if (P.Voxel.IsSet())
	{
		const auto& Capture = P.Voxel->Voxels;
		const auto& Completion = Capture.Completion;
		if (!Completion->TargetReadLease)
		{
			bool bCapacityBlocked = false;
			if (!FSaveGameGenerationReadLease::AcquireGranted(*Completion->TargetDirectory, Capture.Journal.Resources,
				FVoxelGenerationConsumerKey::Section(Capture.WorldEpoch, Capture.TransactionId, 1, {}, 9),
				Completion->TargetReadLease, bCapacityBlocked))
				return Fail(bCapacityBlocked ? ESaveResultCode::CapacityBlocked : ESaveResultCode::CommitFailed,
					TEXT("Original save transaction could not pin its prepared target generation"));
		}
		if (!Completion->TargetReadLease->Matches(*Completion->TargetDirectory) ||
			(Capture.Journal.Resources && !Completion->TargetReadLease->IsResourceAccountedFor(Capture.Journal.Resources.Get(), Capture.WorldEpoch)))
			return Fail(ESaveResultCode::CaptureFailed, TEXT("Prepared target generation lease differs from its original world owner"));
	}
    if (P.ExternalOperation)
    {
        FString Error;
        if (!P.ExternalOperation->PendingLease || !P.ExternalOperation->Store ||
            !P.ExternalOperation->Store->PrepareNative(Error) || !P.ExternalOperation->Store->MarkReadyNative(Error))
            return FSaveOperationResult::Failed(ESaveResultCode::WriteFailed, FText::FromString(Error.IsEmpty() ?
                TEXT("外部世界存储没有完成持久准备。") : Error));
    }
	if (Cancel.load()) return Fail(ESaveResultCode::Busy, TEXT("Save canceled before generation publication"));
	if (!Storage.CommitGeneration(P.SaveId, Next))
		return Fail(ESaveResultCode::CommitFailed, TEXT("Generation commit failed"));
	// 文件代已发布后继续完成 manifest，取消不能把仅 rename 的目录当作提交成功。
	FSaveManifest Manifest = Current;
	Manifest.CurrentGeneration = Next;
	Manifest.LastCommitOperationId = P.OperationId;
	Manifest.CurrentMap = P.CurrentMap;
	Manifest.UpdatedAt = FDateTime::UtcNow();
	for (const auto& M : P.Modules)
		Manifest.ModuleVersions.Add(M.ModuleName, M.Version);
    if (!Storage.WriteManifestAtomic(P.SaveId, Manifest, PendingLease))
    {
        FSaveManifest Observed;
        // rename 后目录刷盘失败可能已经发布；以同 OperationId 的完整 Manifest 为准。
        const bool bObserved = Storage.ReadManifest(P.SaveId, Observed);
        const bool bMatches = bObserved && Observed.LastCommitOperationId == P.OperationId && Observed.CurrentGeneration == Next;
        if (!bObserved || (bMatches && !Storage.IsGenerationComplete(P.SaveId, Next, Observed)))
        {
            if (P.ExternalOperation) P.ExternalOperation->Context.bPublicationUncertain = true;
            return Fail(ESaveResultCode::ConfirmationPending, TEXT("目标发布结果暂时无法确认；数据与 intent 已保留，请重新读取存档结果。"));
        }
        if (!bMatches)
            return Fail(ESaveResultCode::CommitFailed, TEXT("普通存档 Manifest 未发布，原活动世界保持不变。"));
        FString ConfirmError;
        if (!Storage.ConfirmManifestCommit(P.SaveId, P.OperationId, Next, ConfirmError))
        {
            if (P.ExternalOperation) P.ExternalOperation->Context.bPublicationUncertain = true;
            return FSaveOperationResult::Failed(ESaveResultCode::ConfirmationPending, FText::FromString(ConfirmError));
        }
        UE_LOG(LogTemp, Warning, TEXT("Manifest publication initially failed; its exact receipt and durable flush were subsequently confirmed."));
    }
    if (P.ExternalOperation) P.ExternalOperation->bFilesCommitted.store(true);
	if (P.Voxel.IsSet())
	{
		// 仅此实际事务可发布文件代回执；目录 rename 或 Scene 内存捕获都不足以证明提交。
		P.Voxel->Voxels.Completion->bGenerationCommitted = true;
	}
	if (P.bWriteLastActiveSave && !Storage.WriteLastActiveSave(P.SaveId))
	{
		UE_LOG(LogTemp, Warning, TEXT("Save committed, but the original worker could not update last_active.txt."));
	}
	return FSaveOperationResult::Success();
}


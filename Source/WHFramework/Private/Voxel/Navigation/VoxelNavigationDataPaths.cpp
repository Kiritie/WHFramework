#include "Voxel/Navigation/VoxelNavigationData.h"

#include "AI/Navigation/NavAgentInterface.h"
#include "Delegates/DelegateInstancesImpl.h"
#include "Misc/ScopeExit.h"
#include "Voxel/Navigation/VoxelNavigationRepathControl.h"
#include "Voxel/Navigation/VoxelNavigationSystem.h"
#include "Voxel/Rendering/VoxelTerrainFrameContext.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"

void AVoxelNavigationData::TickActor(const float InDeltaSeconds, const ELevelTick InTickType,
	FActorTickFunction& InTickFunction)
{
	// UE5.8 Recast没有额外TickActor逻辑；路径观察/清理/重规划由模块唯一FrameContext推进。
	AActor::TickActor(InDeltaSeconds, InTickType, InTickFunction);
}

bool AVoxelNavigationData::QueueWaitingRepath(const FNavPathSharedPtr& InPath, const ENavPathUpdateType::Type InReason)
{
	if (!InPath) return true;
	const FNavPathRecalculationRequest Request(InPath, InReason);
	if (!RepathRequests.Contains(Request) && RepathRequests.Num() >= EvidenceCapacity) return false;
	RepathRequests.AddUnique(Request);
	InPath->SetManualRepathWaiting(true);
	return true;
}

void AVoxelNavigationData::RemoveRepathControl(FVoxelNavigationRepathControl& InControl)
{
	if (const auto* Entry = PendingRepaths.Find(InControl.ProducerId); Entry && Entry->Get() == &InControl)
	{
		if (const auto* Id = PendingRepathPaths.Find(InControl.PathKey); Id && *Id == InControl.ProducerId)
		{
			PendingRepathPaths.Remove(InControl.PathKey);
		}
		PendingRepaths.Remove(InControl.ProducerId);
	}
}

bool AVoxelNavigationData::AdmitRepath(const FNavPathSharedPtr& InPath, const ENavPathUpdateType::Type InReason)
{
	if (const auto* ExistingId = PendingRepathPaths.Find(InPath.Get()))
	{
		if (const auto* Existing = PendingRepaths.Find(*ExistingId); Existing && InReason == ENavPathUpdateType::NavigationChanged)
		{
			(*Existing)->Reason = InReason;
		}
		return true;
	}
	auto* System = FNavigationSystem::GetCurrent<UVoxelNavigationSystem>(GetWorld());
	if (!System || PendingRepaths.Num() >= EvidenceCapacity || bWorldCloseRequested) return false;
	FPathFindingQuery Query(InPath.ToSharedRef());
	Query.SetPathInstanceToUpdate(InPath);
	using FCallbackInstance = TBaseSPMethodDelegateInstance<false, FVoxelNavigationRepathControl, ESPMode::ThreadSafe,
		void(uint32, ENavigationQueryResult::Type, FNavPathSharedPtr), FDefaultDelegateUserPolicy>;
	const uint64 CallbackBytes = FMemory::QuantizeSize(Align(sizeof(FCallbackInstance), SIZE_T(16)), 16);
	const uint64 Bytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelNavigationRepathControl>() + CallbackBytes;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	const FGuid Producer = FGuid::NewGuid();
	const auto Consumer = FVoxelGenerationConsumerKey::Section(WorldEpoch, Producer, 1, {}, 1);
	if (!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Input, Bytes, Lease)) return false;
	const auto Control = VoxelBoundedAllocation::MakeWorkerShared<FVoxelNavigationRepathControl>(Lease);
	Control->Navigation = this;
	Control->Lease = Lease;
	Control->ProducerId = Producer;
	Control->BindingId = TerrainBindingId;
	Control->WorldEpoch = WorldEpoch;
	Control->Path = InPath;
	Control->PathKey = InPath.Get();
	Control->Reason = InReason;
	Control->EndLocation = Query.EndLocation;
	Control->ExpectedSemanticInputs = FindPathInputs(InPath);
	const auto Callback = FNavPathQueryDelegate::CreateSP(Control.ToSharedRef(), &FVoxelNavigationRepathControl::OnComplete);
	check(Callback.GetAllocatedSize() <= CallbackBytes);
	Control->QueryId = System->FindPathAsync(Query.NavAgentProperties, Query, Callback);
	if (Control->QueryId == INVALID_NAVQUERYID) return false;
	PendingRepaths.Add(Control->ProducerId, Control);
	PendingRepathPaths.Add(InPath.Get(), Control->ProducerId);
	InPath->SetManualRepathWaiting(true);
	return true;
}

void AVoxelNavigationData::CompleteRepath(FVoxelNavigationRepathControl& InControl, const uint32 InQueryId,
	const ENavigationQueryResult::Type InResult, FNavPathSharedPtr InPath)
{
	check(IsInGameThread());
	const auto* Entry = PendingRepaths.Find(InControl.ProducerId);
	if (!Entry || Entry->Get() != &InControl || InControl.QueryId != InQueryId) return;
	// 真实delegate只写入原控制的单个槽；观察者通知和重试由共享FrameContext结算。
	InControl.CompletionResult = InResult;
	InControl.CompletionPath = InPath;
	InControl.bCompletionPending = true;
}

void AVoxelNavigationData::FinalizeRepath(FVoxelNavigationRepathControl& InControl)
{
	const auto* Entry = PendingRepaths.Find(InControl.ProducerId);
	if (!Entry || Entry->Get() != &InControl || !InControl.bCompletionPending) return;
	const auto Control = *Entry;
	const auto Target = Control->Path.Pin();
	const auto InPath = Control->CompletionPath.Pin();
	const auto InResult = Control->CompletionResult;
	if (Control->WorldEpoch != WorldEpoch || Control->BindingId != TerrainBindingId || bWorldCloseRequested ||
		!Target || (InPath && InPath != Target))
	{
		RemoveRepathControl(*Control);
		return;
	}
	if (!TryBeginNativeWrite())
	{
		return;
	}
	ON_SCOPE_EXIT { FinishNativeWrite(); };
	if (FindPathInputs(Target) != Control->ExpectedSemanticInputs)
	{
		// 另一实际尝试已接管路径，旧请求不能修改其状态或重新排队。
		RemoveRepathControl(*Control);
		return;
	}
	const FPathFindingQuery CurrentQuery(Target.ToSharedRef());
	const bool bGoalChanged = !CurrentQuery.EndLocation.Equals(Control->EndLocation, 1.0);
	const bool bUsableResult = InResult == ENavigationQueryResult::Success ||
		(InResult != ENavigationQueryResult::Error && InPath == Target && Target->IsReady() && Target->IsPartial());
	// 原delegate与帧结算之间也可能撤销输入；成功枚举不能恢复已退役的路径。
	const bool bInputRevoked = bUsableResult && !IsPathInputAttemptCurrent(Target, Control->ExpectedSemanticInputs);
	if (bGoalChanged || bInputRevoked || (InResult != ENavigationQueryResult::Success && Control->bWaiting))
	{
		if (bGoalChanged)
		{
			const bool bIgnored = Target->GetIgnoreInvalidation();
			const bool bAuto = Target->WillRecalculateOnInvalidation();
			Target->SetIgnoreInvalidation(false);
			Target->EnableRecalculationOnInvalidation(false);
			Target->Invalidate();
			Target->SetIgnoreInvalidation(bIgnored);
			Target->EnableRecalculationOnInvalidation(bAuto);
			Control->Reason = ENavPathUpdateType::GoalMoved;
		}
		Control->bRequeuePending = true;
		Control->bCompletionPending = false;
		if (QueueWaitingRepath(Target, Control->Reason)) RemoveRepathControl(*Control);
		return;
	}
	if (bUsableResult && Target->IsUpToDate() && (Target->IsReady() || Target->GetPathPoints().Num() == 1))
	{
		if (!ActivePathsLock.TryLock()) return;
		if (!ActivePaths.Contains(FNavPathWeakPtr(Target)))
		{
			if (ActivePaths.Num() >= EvidenceCapacity)
			{
				ActivePathsLock.Unlock();
				return;
			}
			ActivePaths.Add(Target);
		}
		ActivePathsLock.Unlock();
		RemoveRepathControl(*Control);
		Target->SetTimeStamp(GetWorldTimeStamp());
		Target->UpdateLastRepathGoalLocation();
		Target->DoneUpdating(Control->Reason);
	}
	else
	{
		RemoveRepathControl(*Control);
		Target->SetTimeStamp(GetWorldTimeStamp());
		Target->RePathFailed();
	}
}

void AVoxelNavigationData::AdvancePathMaintenance(FVoxelTerrainFrameContext& InFrame)
{
	check(IsInGameThread());
	if (!StorageLease || !Resources || bWorldCloseRequested || !InFrame.IsActive() ||
		InFrame.GetWorldEpoch() != WorldEpoch || PathMaintenanceFrame == InFrame.GetFrameCounter()) return;
	if (const auto* System = FNavigationSystem::GetCurrent<UVoxelNavigationSystem>(GetWorld());
		System && System->IsTerrainQueryBatchRunning()) return;
	if (!TryBeginNativeWrite()) return;
	ON_SCOPE_EXIT { FinishNativeWrite(); };
	PathMaintenanceFrame = InFrame.GetFrameCounter();
	const double Deadline = InFrame.Deadline(0.15);
	const double Now = GetWorldTimeStamp();
	if (LastPathObservationWorldTime >= 0.0 && NextObservedPathsTickInSeconds >= 0.f)
	{
		NextObservedPathsTickInSeconds -= float(FMath::Max(0.0, Now - LastPathObservationWorldTime));
	}
	LastPathObservationWorldTime = Now;
	if (ObservedPaths.IsEmpty()) ObservedPathsRemaining = 0;
	if (ObservedPathsRemaining == 0 && !ObservedPaths.IsEmpty() && NextObservedPathsTickInSeconds <= 0.f)
	{
		ObservedPathsRemaining = ObservedPaths.Num();
		NextObservedPathsTickInSeconds = ObservedPathsTickInterval;
	}
	for (int32 Step = 0; Step < 32; ++Step)
	{
		const double Started = FPlatformTime::Seconds();
		if (Started >= Deadline || !InFrame.GetPublishBudget().TryStart(EVoxelPublishPhase::Admission,
			Started, PathMaintenanceCostMilliseconds)) break;
		const uint8 Phase = PathMaintenancePhase;
		PathMaintenancePhase = (PathMaintenancePhase + 1) % 3;
		if (Phase == 0 && ActivePathsLock.TryLock())
		{
			if (!ActivePaths.IsEmpty())
			{
				ActivePathCursor %= ActivePaths.Num();
				if (!ActivePaths[ActivePathCursor].IsValid()) ActivePaths.RemoveAtSwap(ActivePathCursor, 1, EAllowShrinking::No);
				else ++ActivePathCursor;
			}
			ActivePathsLock.Unlock();
		}
		if (Phase == 1 && ObservedPathsRemaining > 0 && !ObservedPaths.IsEmpty())
		{
			ObservedPathCursor %= ObservedPaths.Num();
			const auto Path = ObservedPaths[ObservedPathCursor].Pin();
			if (!Path || Path->GetNavigationDataUsed() != this || !FindPathInputs(Path))
			{
				ObservedPaths.RemoveAtSwap(ObservedPathCursor, 1, EAllowShrinking::No);
			}
			else
			{
				const auto Result = Path->TickPathObservation();
				if (Result == EPathObservationResult::NoLongerObserving)
				{
					ObservedPaths.RemoveAtSwap(ObservedPathCursor, 1, EAllowShrinking::No);
				}
				else if (Result == EPathObservationResult::RequestRepath && !QueueWaitingRepath(Path, ENavPathUpdateType::GoalMoved))
				{
					NextObservedPathsTickInSeconds = 0.f;
				}
				else ++ObservedPathCursor;
			}
			--ObservedPathsRemaining;
			if (ObservedPaths.IsEmpty()) ObservedPathsRemaining = 0;
		}
		if (Phase == 2)
		{
			if (!PendingRepaths.IsEmpty())
			{
				RepathReturnCursor %= PendingRepaths.GetMaxIndex();
				const auto Id = FSetElementId::FromInteger(RepathReturnCursor++);
				if (PendingRepaths.IsValidId(Id))
				{
					const auto Control = PendingRepaths.Get(Id).Value;
					if (Control->bCompletionPending) FinalizeRepath(*Control);
					const auto Path = Control->Path.Pin();
					if (Control->bRequeuePending && (!Path || QueueWaitingRepath(Path, Control->Reason))) RemoveRepathControl(*Control);
				}
			}
			if (!RepathRequests.IsEmpty())
			{
				RepathCursor %= RepathRequests.Num();
				const auto Request = RepathRequests[RepathCursor];
				const auto Path = Request.Path.Pin();
				const auto* Agent = Path ? Cast<const INavAgentInterface>(Path->GetQuerier()) : nullptr;
				if (!Path || Path->GetNavigationDataUsed() != this || !FindPathInputs(Path))
				{
					RepathRequests.RemoveAtSwap(RepathCursor, 1, EAllowShrinking::No);
				}
				else if ((!Agent || !Agent->ShouldPostponePathUpdates()) && AdmitRepath(Path, Request.Reason))
				{
					RepathRequests.RemoveAtSwap(RepathCursor, 1, EAllowShrinking::No);
				}
				else ++RepathCursor;
			}
		}
		const double Finished = FPlatformTime::Seconds();
		PathMaintenanceCostMilliseconds = FMath::Lerp(PathMaintenanceCostMilliseconds, (Finished - Started) * 1000.0, 0.2);
		InFrame.GetPublishBudget().Finish(EVoxelPublishPhase::Admission, Started, Finished);
	}
}

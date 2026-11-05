#include "Voxel/Collision/VoxelCollisionPresenter.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"
#include "Physics/PhysicsInterfaceCore.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "PhysicsEngine/BodySetup.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "PBDRigidsSolver.h"
#include "Chaos/SimCallbackObject.h"
#include "Voxel/Chunks/VoxelSectionKey.h"
#include "Voxel/Components/VoxelCollisionComponent.h"
#include "Voxel/Collision/VoxelPhysicsRetirement.h"
#include "Voxel/Collision/VoxelCollisionBodyIndex.h"
#include "Voxel/Geometry/VoxelCollisionBuilder.h"
#include "Voxel/Network/VoxelOverlayReplica.h"
#include "Voxel/Rendering/VoxelFineShardBuild.h"
#include "Voxel/Runtime/VoxelWorldRuntime.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/VoxelModule.h"

struct FVoxelCollisionEvidenceNode
{
	TSharedPtr<const FVoxelCollisionEvidenceNode, ESPMode::ThreadSafe> Children[8];
	TOptional<FVoxelPhysicsReceipt> Receipt;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
};

struct FVoxelCollisionPresenter::FPhysicsSyncCommand
{
	TSharedPtr<FPhysicsSync, ESPMode::ThreadSafe> Sync;
	Chaos::FPBDRigidsSolver* Solver = nullptr;
	TSharedPtr<FVoxelPhysicsRetirementTicket, ESPMode::ThreadSafe> Ticket;

	void operator()() const
	{
		if (Sync->bComplete.Load()) return;
		Sync->Step.Store(Solver->GetCurrentFrame());
		Sync->bCommitted.Store(!Ticket || Ticket->ConfirmPhysicsCommitted(*Solver));
		Sync->bComplete.Store(true);
	}
};

struct FVoxelCollisionPresenter::FControlState
{
	TVoxelCollisionBodyIndex<FBodyRecord> Index;
	FBodyRecord* ActiveHead = nullptr;
	FBodyRecord* ActiveCursor = nullptr;
	int32 ActiveCount = 0;

#if !UE_BUILD_SHIPPING
	struct FServiceWindow
	{
		uint64 Ticks = 0;
		uint64 Scans[4] = {};
		uint64 Candidates[4] = {};
		uint64 WaitingCook = 0;
		uint64 WaitingPhysics = 0;
		uint64 AdvanceCalls = 0;
		uint64 TicksPast16 = 0;
		uint64 ExtraAdvanceCalls = 0;
		uint64 ExitBeforeCollection = 0;
		uint64 ExitAfterCollection = 0;
		uint64 ExitNoCandidate = 0;
		uint64 ExitStepLimit = 0;
		uint64 ExitOther = 0;

		void Accumulate(const FServiceWindow& InWindow)
		{
			Ticks += InWindow.Ticks;
			for (int32 CounterIndex = 0; CounterIndex < 4; ++CounterIndex)
			{
				Scans[CounterIndex] += InWindow.Scans[CounterIndex];
				Candidates[CounterIndex] += InWindow.Candidates[CounterIndex];
			}
			WaitingCook += InWindow.WaitingCook;
			WaitingPhysics += InWindow.WaitingPhysics;
			AdvanceCalls += InWindow.AdvanceCalls;
			TicksPast16 += InWindow.TicksPast16;
			ExtraAdvanceCalls += InWindow.ExtraAdvanceCalls;
			ExitBeforeCollection += InWindow.ExitBeforeCollection;
			ExitAfterCollection += InWindow.ExitAfterCollection;
			ExitNoCandidate += InWindow.ExitNoCandidate;
			ExitStepLimit += InWindow.ExitStepLimit;
			ExitOther += InWindow.ExitOther;
		}
	};

	FServiceWindow ServiceWindow;
#endif

	void Activate(FBodyRecord& InBody)
	{
		if (InBody.bActive) return;
		InBody.ActiveNext = ActiveHead;
		if (ActiveHead) ActiveHead->ActivePrevious = &InBody;
		ActiveHead = &InBody;
		if (!ActiveCursor) ActiveCursor = &InBody;
		InBody.bActive = true;
		++ActiveCount;
	}

	void Deactivate(FBodyRecord& InBody)
	{
		if (!InBody.bActive) return;
		FBodyRecord* Next = InBody.ActiveNext;
		if (InBody.ActivePrevious) InBody.ActivePrevious->ActiveNext = Next;
		else ActiveHead = Next;
		if (Next) Next->ActivePrevious = InBody.ActivePrevious;
		if (ActiveCursor == &InBody) ActiveCursor = Next ? Next : ActiveHead;
		InBody.ActivePrevious = InBody.ActiveNext = nullptr;
		InBody.bActive = false;
		--ActiveCount;
		check(ActiveCount >= 0 && (ActiveCount != 0 || (!ActiveHead && !ActiveCursor)));
	}

	FBodyRecord* NextActive()
	{
		FBodyRecord* Body = ActiveCursor;
		if (Body) ActiveCursor = Body->ActiveNext ? Body->ActiveNext : ActiveHead;
		return Body;
	}
};

namespace
{
	using FNodePtr = TSharedPtr<const FVoxelCollisionEvidenceNode, ESPMode::ThreadSafe>;
	uint8 CollisionBranch(const FIntVector& InKey, const int32 InBit)
	{
		return (((uint32(InKey.X) ^ 0x80000000u) >> InBit) & 1u) |
			((((uint32(InKey.Y) ^ 0x80000000u) >> InBit) & 1u) << 1) |
			((((uint32(InKey.Z) ^ 0x80000000u) >> InBit) & 1u) << 2);
	}
	uint64 EvidenceNodeBytes()
	{
		return VoxelBoundedAllocation::WorkerSharedBytes<FVoxelCollisionEvidenceNode>();
	}
	FNodePtr ChangeEvidence(const FNodePtr& InBase, const FIntVector& InKey, const int32 InBit,
		const TOptional<FVoxelPhysicsReceipt>& InReceipt,
		const TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& InLease)
	{
		if (InBit < 0 && !InReceipt.IsSet()) return nullptr;
		auto Lease = InLease->Split(EvidenceNodeBytes());
		check(Lease);
		auto Node = VoxelBoundedAllocation::MakeWorkerShared<FVoxelCollisionEvidenceNode>(Lease);
		Node->Lease = MoveTemp(Lease);
		if (InBase)
		{
			for (int32 Index = 0; Index < 8; ++Index) Node->Children[Index] = InBase->Children[Index];
			Node->Receipt = InBase->Receipt;
		}
		if (InBit < 0) Node->Receipt = InReceipt;
		else
		{
			const uint8 Index = CollisionBranch(InKey, InBit);
			Node->Children[Index] = ChangeEvidence(Node->Children[Index], InKey, InBit - 1, InReceipt, InLease);
			bool bHasChild = false;
			for (const auto& Child : Node->Children) bHasChild |= Child.IsValid();
			if (!bHasChild) return nullptr;
		}
		return Node;
	}
}

const FVoxelPhysicsReceipt* FVoxelCollisionCoverageSnapshot::Find(const FIntVector& InSection) const
{
	const FVoxelCollisionEvidenceNode* Node = Root.Get();
	for (int32 Bit = 31; Bit >= 0 && Node; --Bit) Node = Node->Children[CollisionBranch(InSection, Bit)].Get();
	return Node && Node->Receipt.IsSet() ? &Node->Receipt.GetValue() : nullptr;
}

FVoxelCollisionPresenter::FVoxelCollisionPresenter(UVoxelModule& InModule,
	FVoxelTaskScheduler& InScheduler, const uint64 InWorldEpoch)
	: Module(InModule), Scheduler(InScheduler), WorldEpoch(InWorldEpoch)
{
	Selection.BindResources(Scheduler.GetTerrainResources(),
		FVoxelGenerationConsumerKey::Section(WorldEpoch, OwnerId, 1, {}, 1));
}

FVoxelCollisionPresenter::FBodyRecord* FVoxelCollisionPresenter::FindBody(const FIntVector& InSection) const
{
	return Control ? Control->Index.Find(InSection) : nullptr;
}

FVoxelCollisionPresenter::FBodyRecord* FVoxelCollisionPresenter::FindOrAddBody(
	const FVoxelGenerationConsumerKey& InConsumer)
{
	const auto Resources = Scheduler.GetTerrainResources();
	if (!Control)
	{
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
		if (!Resources->Acquire(InConsumer, EVoxelTerrainResourcePool::Resident,
			VoxelBoundedAllocation::WorkerSharedBytes<FControlState>(), Lease)) return nullptr;
		Control = VoxelBoundedAllocation::MakeWorkerShared<FControlState>(MoveTemp(Lease));
	}
	bool bAdded = false;
	auto* Body = Control->Index.FindOrAdd(InConsumer.Min, *Resources, InConsumer, bAdded);
	if (bAdded)
	{
		check(NextBodyQueueSerial < MAX_uint64);
		Body->QueueSerial = ++NextBodyQueueSerial;
		Body->QueuedAt = FPlatformTime::Seconds();
	}
	return Body;
}

void FVoxelCollisionPresenter::RemoveBody(FBodyRecord& InBody)
{
	check(Control);
	Control->Deactivate(InBody);
	Control->Index.Remove(InBody);
}

bool FVoxelCollisionPresenter::IsDemanded(const FIntVector& InSection) const
{
	const auto* Demand = Module.GetCurrentInterest().Exact.Find(InSection);
	// 实际 Nav 查询/路径尚未归还时，Source 和 Simulation 退出也不撤销其物理输入。
	return Module.IsSimulationResident(InSection) || Module.IsNavigationResident(InSection) ||
		(Demand && (Demand->bCollision || Demand->bWarmupCollision));
}

bool FVoxelCollisionPresenter::EnsureNavigationDemand(const FIntVector& InSection)
{
	check(IsInGameThread());
	const auto* Data = Module.GetRuntime() ? Module.GetRuntime()->FindSection(InSection) : nullptr;
	if (!Data) return false;
	const auto Consumer = FVoxelGenerationConsumerKey::Section(WorldEpoch, OwnerId, Data->Incarnation, InSection, ++NextLeaseSerial);
	auto* Body = FindOrAddBody(Consumer);
	if (!Body) return false;
	if (Body->RequestedStamp.OwnerIncarnation == 0) Body->RequestedStamp = CaptureInputStamp(InSection);
	if (!IsReady(InSection)) Control->Activate(*Body);
	return true;
}

bool FVoxelPhysicsReceipt::MatchesInput(const FVoxelTaskStamp& InInput, const FGuid& InSceneEpoch) const
{
	return WorldEpoch != 0 && PresenterOwnerId.IsValid() && SceneEpoch.IsValid() && SectionIncarnation != 0 &&
		WorldEpoch == InInput.WorldEpoch && PresenterOwnerId == InInput.OwnerId &&
		SceneEpoch == InSceneEpoch && PhysicsStep != INDEX_NONE && BodyGeneration != 0 &&
		Section == InInput.Section && SectionToken == InInput.Token && SectionIncarnation == InInput.OwnerIncarnation &&
		DataRevision == InInput.Revision && ContentRevision == InInput.GeometryRevision &&
		RegistryRevision == InInput.RegistryRevision && ShapeRevision == InInput.ShapeRevision &&
		NeighborRevision == InInput.NeighborRevision;
}

FVoxelTaskStamp FVoxelCollisionPresenter::CaptureInputStamp(const FIntVector& InSection) const
{
	const auto* Data = Module.GetRuntime()->FindSection(InSection);
	check(Data);
	FVoxelTaskStamp Stamp;
	Stamp.OwnerId = OwnerId;
	Stamp.OwnerIncarnation = Data->Incarnation;
	Stamp.WorldEpoch = WorldEpoch;
	Stamp.Token = Data->Stamp.Token;
	Stamp.Revision = Data->CommittedRevision;
	Stamp.Section = InSection;
	Stamp.GeometryRevision = Module.GetRuntime()->GetChangeHierarchy().GetSectionRevision(InSection);
	Stamp.NeighborRevision = Module.GetRuntime()->GetChangeHierarchy().GetNeighborSectionRevision(InSection);
	Stamp.RegistryRevision = Module.GetRegistry()->Hash;
	Stamp.ShapeRevision = Module.GetShapes()->GetContentHash();
	return Stamp;
}

TSharedPtr<FVoxelCollisionPresenter::FPhysicsSync, ESPMode::ThreadSafe>
FVoxelCollisionPresenter::PreparePhysicsSync(const FIntVector& InSection)
{
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	const uint64 Bytes = VoxelBoundedAllocation::SharedBytes<FPhysicsSync>() +
		FMemory::QuantizeSize(sizeof(Chaos::FSimCallbackCommandObject), alignof(Chaos::FSimCallbackCommandObject)) +
		VoxelBoundedAllocation::FunctionBytes<FPhysicsSyncCommand>();
	const auto* Body = FindBody(InSection);
	check(Body);
	const uint64 Incarnation = Body->RequestedStamp.OwnerIncarnation;
	const auto Consumer = FVoxelGenerationConsumerKey::Section(WorldEpoch, OwnerId, Incarnation, InSection, ++NextLeaseSerial);
	if (!Scheduler.GetTerrainResources()->Acquire(Consumer, EVoxelTerrainResourcePool::Input, Bytes, Lease)) return nullptr;
	auto Sync = MakeShared<FPhysicsSync, ESPMode::ThreadSafe>();
	Sync->MetadataLease = MoveTemp(Lease);
	return Sync;
}

void FVoxelCollisionPresenter::Tick(const TMap<FIntVector, FVoxelExactDemand>& InDemand, const uint64 InInterestRevision)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_CollisionServiceTick);
#if !UE_BUILD_SHIPPING
	FControlState::FServiceWindow TickWindow;
	TickWindow.Ticks = 1;
	TickWindow.ExitOther = 1;
	ON_SCOPE_EXIT
	{
		if (Control) Control->ServiceWindow.Accumulate(TickWindow);
	};
#endif
	const auto Snapshot = Module.GetCurrentInterestSnapshot();
	if (!Snapshot || !Snapshot->PublicationInputs) return;
	const auto& Keys = Snapshot->PublicationInputs->RuntimeSections;
	const double Deadline = Module.GetTerrainFrameContext().Deadline(0.5);
#if !UE_BUILD_SHIPPING
	if (FPlatformTime::Seconds() >= NextActiveDiagnostic)
	{
		UE_LOG(LogTemp, Display, TEXT("Voxel collision work: records=%d active=%d emptyBuilds=%llu nonemptyBuilds=%llu sharedMeshBuilds=%llu emptyReceipts=%llu nonemptyReceipts=%llu"),
			Control ? Control->Index.Num() : 0, Control ? Control->ActiveCount : 0,
			EmptyBuilds, NonemptyBuilds, SharedMeshBuilds, EmptyReceipts, NonemptyReceipts);
		if (Control)
		{
			const auto& Window = Control->ServiceWindow;
			UE_LOG(LogTemp, Display, TEXT("Voxel collision service window: preControlTicksExcluded=1 ticks=%llu criticalScans=%llu criticalCandidates=%llu demandScans=%llu demandCandidates=%llu activeScans=%llu activeCandidates=%llu indexScans=%llu indexCandidates=%llu waitingCook=%llu waitingPhysics=%llu advanceCalls=%llu ticksPast16=%llu extraAdvanceCalls=%llu exitBeforeCollection=%llu exitAfterCollection=%llu exitSelectNoRunnable=%llu exitStepLimit=%llu exitOther=%llu"),
				Window.Ticks, Window.Scans[0], Window.Candidates[0], Window.Scans[1], Window.Candidates[1],
				Window.Scans[2], Window.Candidates[2], Window.Scans[3], Window.Candidates[3], Window.WaitingCook,
				Window.WaitingPhysics, Window.AdvanceCalls, Window.TicksPast16, Window.ExtraAdvanceCalls,
				Window.ExitBeforeCollection, Window.ExitAfterCollection, Window.ExitNoCandidate, Window.ExitStepLimit, Window.ExitOther);
			Control->ServiceWindow = {};
		}
		NextActiveDiagnostic = FPlatformTime::Seconds() + 5.0;
	}
#endif
	struct FOperation { FIntVector Section; bool bBody = false; };
	if (FPlatformTime::Seconds() >= Deadline)
	{
#if !UE_BUILD_SHIPPING
		TickWindow.ExitOther = 0;
		TickWindow.ExitBeforeCollection = 1;
#endif
		return;
	}
	TSharedPtr<FPhysicsSync, ESPMode::ThreadSafe> EmptySync;
	ON_SCOPE_EXIT
	{
		if (!EmptySync) return;
		FPhysScene* Scene = Module.GetWorld() ? Module.GetWorld()->GetPhysicsScene() : nullptr;
		if (Scene && Scene->GetSolver()) Scene->GetSolver()->EnqueueCommandImmediate(FPhysicsSyncCommand{EmptySync, Scene->GetSolver()});
		else EmptySync->bComplete.Store(true);
	};
	TArray<FVoxelTerrainServiceCandidate, TInlineAllocator<128>> Candidates;
	TArray<FOperation, TInlineAllocator<128>> Operations;
	const double QueuedAt = FPlatformTime::Seconds();
	const auto NeighborsReadyForBackground = [&](const FIntVector& Key, const FVoxelExactDemand& Demand)
	{
		if (Demand.bMovementCriticalCollision || Demand.bWarmupCollision || Demand.bSimulation) return true;
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			for (const int32 Direction : {-1, 1})
			{
				const int64 Coordinate = int64(Key[Axis]) + Direction;
				if (Coordinate < MIN_int32 || Coordinate > MAX_int32) continue;
				FIntVector Neighbor = Key;
				Neighbor[Axis] = int32(Coordinate);
				const auto* NeighborDemand = InDemand.Find(Neighbor);
				if (!NeighborDemand || !NeighborDemand->bData) continue;
				const auto* NeighborData = Module.GetRuntime()->FindSection(Neighbor);
				if (!NeighborData || NeighborData->Status != EVoxelSectionStatus::DataReady) return false;
			}
		}
		return true;
	};
	const auto HasSharedFineGeometry = [&](const FIntVector& Key)
	{
		const auto* View = Module.GetViewManager();
		if (!View) return false;
		bool bExpected = false;
		const auto Geometry = View->FindFineCollisionGeometry(Key, bExpected);
		FVoxelSectionCollisionResult Probe;
		return Geometry && Probe.BindFineGeometry(Geometry, Key);
	};
	const auto AddDemandCandidate = [&](const FIntVector& Key)
	{
		const auto* Demand = InDemand.Find(Key);
		const auto* Data = Module.GetRuntime()->FindSection(Key);
		const FBodyRecord* Body = FindBody(Key);
		if (!Demand || (!Demand->bCollision && !Demand->bWarmupCollision) || !Data ||
			Data->Status != EVoxelSectionStatus::DataReady ||
			(IsReady(Key, Data, Body) && (!Body || Body->OcclusionRefreshMask == 0))) return;
		if (Body && (Body->Prepared || Body->Sync))
		{
			if ((Body->Sync && !Body->Sync->bComplete.Load()) ||
				(Body->PreparingComponent && Body->PreparingComponent->IsDetachedCookRunning())) return;
			auto& Candidate = Candidates.AddDefaulted_GetRef();
			Candidate.Index = Operations.Add({Key, true});
			Candidate.Priority = Demand->ServicePriority;
			Candidate.FirstQueuedSeconds = Body->QueuedAt;
			Candidate.Sequence = Body->QueueSerial;
			Candidate.EstimatedMicroseconds = StepCostMicroseconds;
			Candidate.bRunnable = true;
			return;
		}
		const auto Stamp = CaptureInputStamp(Key);
		if (Scheduler.Has(Stamp, EVoxelTaskKind::BuildCollision) ||
			(Body && Body->bHasFailure && Body->FailedStamp == Stamp)) return;
		if (!NeighborsReadyForBackground(Key, *Demand) && !HasSharedFineGeometry(Key)) return;
		auto& Candidate = Candidates.AddDefaulted_GetRef();
		Candidate.Index = Operations.Add({Key, false});
		Candidate.Priority = Demand->ServicePriority;
		Candidate.FirstQueuedSeconds = Body && Body->RequestedStamp == Stamp ? Body->QueuedAt : QueuedAt;
		Candidate.Sequence = Data->Stamp.Token;
		Candidate.EstimatedMicroseconds = StepCostMicroseconds;
		Candidate.bRunnable = true;
	};
	const auto AddBodyCandidate = [&](FBodyRecord* Body)
	{
		if (!Body) return;
		if (Body->Sync && !Body->Sync->bComplete.Load())
		{
#if !UE_BUILD_SHIPPING
			if (Body->bActive) ++TickWindow.WaitingPhysics;
#endif
			return;
		}
		if (Body->PreparingComponent && Body->PreparingComponent->IsDetachedCookRunning())
		{
#if !UE_BUILD_SHIPPING
			if (Body->bActive) ++TickWindow.WaitingCook;
#endif
			return;
		}
		const FIntVector Key = Body->Section;
		const bool bGenerate = !Body->Prepared && !Body->Sync && !Body->bRetiring && IsDemanded(Key);
		if (bGenerate)
		{
			const auto* Data = Module.GetRuntime()->FindSection(Key);
			if (!Data || Data->Status != EVoxelSectionStatus::DataReady) return;
			if (IsReady(Key, Data, Body) && Body->OcclusionRefreshMask == 0)
			{
				Control->Deactivate(*Body);
				return;
			}
			const auto Stamp = CaptureInputStamp(Key);
			if (Body->bHasFailure && Body->FailedStamp == Stamp)
			{
				Control->Deactivate(*Body);
				return;
			}
			if (Scheduler.Has(Stamp, EVoxelTaskKind::BuildCollision)) return;
			if (const auto* Demand = InDemand.Find(Key); Demand &&
				!NeighborsReadyForBackground(Key, *Demand) && !HasSharedFineGeometry(Key)) return;
		}
		auto& Candidate = Candidates.AddDefaulted_GetRef();
		Candidate.Index = Operations.Add({Key, !bGenerate});
		if (const auto* Demand = InDemand.Find(Key)) Candidate.Priority = Demand->ServicePriority;
		else if (bGenerate) Candidate.Priority = {EVoxelTerrainServiceBand::P2, 0.0, 0.0, 0, OwnerId};
		else Candidate.Priority = {EVoxelTerrainServiceBand::P6, 0.0, 0.0, MAX_int32, OwnerId};
		Candidate.FirstQueuedSeconds = Body->QueuedAt;
		Candidate.Sequence = Body->QueueSerial;
		Candidate.EstimatedMicroseconds = StepCostMicroseconds;
		Candidate.bRunnable = true;
	};
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_CollisionCollectCandidates);
		// 全窗口碰撞可能包含数万个 Section；关键移动需求须独立轮询，不能等待全表扫描。
		const TArray<int32>& Critical = Snapshot->ExactAdmissionLanes[0];
#if !UE_BUILD_SHIPPING
		int32 PreviousCandidates = Candidates.Num();
#endif
		for (int32 Scan = 0; Scan < FMath::Min(32, Critical.Num()); ++Scan)
		{
#if !UE_BUILD_SHIPPING
			++TickWindow.Scans[0];
#endif
			CriticalDemandCursor %= Critical.Num();
			AddDemandCandidate(Snapshot->ExactOrder[Critical[CriticalDemandCursor++]]);
		}
#if !UE_BUILD_SHIPPING
		TickWindow.Candidates[0] = Candidates.Num() - PreviousCandidates;
		PreviousCandidates = Candidates.Num();
#endif
		for (int32 Scan = 0; Scan < FMath::Min(32, Keys.Num()); ++Scan)
		{
#if !UE_BUILD_SHIPPING
			++TickWindow.Scans[1];
#endif
			DemandCursor %= Keys.Num();
			AddDemandCandidate(Keys[DemandCursor++]);
		}
#if !UE_BUILD_SHIPPING
		TickWindow.Candidates[1] = Candidates.Num() - PreviousCandidates;
		PreviousCandidates = Candidates.Num();
#endif
		// 已提交的碰撞工作直接参与同一个服务选择，避免被数万个已安装记录淹没。
		const int32 ActiveScans = Control ? FMath::Min(32, Control->ActiveCount) : 0;
		for (int32 Scan = 0; Control && Scan < ActiveScans && Control->ActiveCount > 0; ++Scan)
		{
#if !UE_BUILD_SHIPPING
			++TickWindow.Scans[2];
#endif
			auto* Body = Control->NextActive();
			check(Body);
			AddBodyCandidate(Body);
		}
#if !UE_BUILD_SHIPPING
		TickWindow.Candidates[2] = Candidates.Num() - PreviousCandidates;
		PreviousCandidates = Candidates.Num();
#endif
		for (int32 Scan = 0; Control && Scan < FMath::Min(32, Control->Index.Num()); ++Scan)
		{
#if !UE_BUILD_SHIPPING
			++TickWindow.Scans[3];
#endif
			auto* Body = Control->Index.NextForScan();
			check(Body);
			if (!Body->bActive) AddBodyCandidate(Body);
		}
#if !UE_BUILD_SHIPPING
		TickWindow.Candidates[3] = Candidates.Num() - PreviousCandidates;
#endif
	}
	// 候选每帧采样一次；同一 Section 本帧只推进一步，避免每步重复扫描整个候选集。
	constexpr int32 MaxCollisionStepsPerTick = 32;
#if !UE_BUILD_SHIPPING
	constexpr int32 PreviousCollisionStepsPerTick = 16;
#endif
	int32 Work = 0;
	for (; Work < MaxCollisionStepsPerTick && FPlatformTime::Seconds() < Deadline; ++Work)
	{
		const double Now = FPlatformTime::Seconds();
		FVoxelTerrainServiceCapacity Capacity;
		Capacity.bAllowOverdueOpportunity = Module.GetTerrainFrameContext().IsOverdueOpportunityAvailable();
		FVoxelTerrainServiceCharge Charge;
		int32 Chosen;
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_CollisionSelectCandidate);
			Chosen = Selection.Select(Candidates, Capacity, Now, Charge);
		}
		if (Chosen == INDEX_NONE)
		{
#if !UE_BUILD_SHIPPING
			if (!Candidates.ContainsByPredicate([](const FVoxelTerrainServiceCandidate& Candidate) { return Candidate.bRunnable; }))
			{
				TickWindow.ExitOther = 0;
				TickWindow.ExitNoCandidate = 1;
			}
#endif
			return;
		}
		if (FPlatformTime::Seconds() >= Deadline)
		{
			Selection.RefundBeforeStart(Charge.Serial);
#if !UE_BUILD_SHIPPING
			TickWindow.ExitOther = 0;
			TickWindow.ExitAfterCollection = 1;
#endif
			return;
		}
#if !UE_BUILD_SHIPPING
		if (Work == PreviousCollisionStepsPerTick) ++TickWindow.TicksPast16;
#endif
		if (Charge.bOverdueOpportunity) Module.GetTerrainFrameContext().ConsumeOverdueOpportunity();
		const double Start = FPlatformTime::Seconds();
		const auto Operation = Operations[Chosen];
		for (auto& Candidate : Candidates)
		{
			if (Candidate.bRunnable && Operations[Candidate.Index].Section == Operation.Section)
				Candidate.bRunnable = false;
		}
		if (Operation.bBody)
		{
			if (auto* Body = FindBody(Operation.Section))
			{
#if !UE_BUILD_SHIPPING
				++TickWindow.AdvanceCalls;
				if (Work >= PreviousCollisionStepsPerTick) ++TickWindow.ExtraAdvanceCalls;
#endif
				AdvanceBody(Operation.Section, *Body, EmptySync);
			}
			if (auto* Body = FindBody(Operation.Section); Body && !Body->Prepared && !Body->Sync &&
				((IsReady(Operation.Section) && Body->OcclusionRefreshMask == 0) || Body->bHasFailure)) Control->Deactivate(*Body);
		}
		else if (const auto* Data = Module.GetRuntime()->FindSection(Operation.Section))
		{
#if !UE_BUILD_SHIPPING
			++TickWindow.AdvanceCalls;
			if (Work >= PreviousCollisionStepsPerTick) ++TickWindow.ExtraAdvanceCalls;
#endif
			const auto* Demand = InDemand.Find(Operation.Section);
			FVoxelExactDemand NavigationDemand;
			NavigationDemand.bCollision = true;
			NavigationDemand.ServicePriority = {EVoxelTerrainServiceBand::P2, 0.0, 0.0, 0, OwnerId};
			RequestCollision(Operation.Section, Data->CommittedRevision,
				Demand && (Demand->bCollision || Demand->bWarmupCollision) ? *Demand : NavigationDemand);
		}
		const double Cost = FMath::Max(1.0, (FPlatformTime::Seconds() - Start) * 1000000.0);
		verify(Selection.Settle(Charge.Serial, Cost));
		StepCostMicroseconds = FMath::Lerp(StepCostMicroseconds, Cost, 0.15);
	}
#if !UE_BUILD_SHIPPING
	TickWindow.ExitOther = 0;
	if (Work == MaxCollisionStepsPerTick) TickWindow.ExitStepLimit = 1;
	else TickWindow.ExitAfterCollection = 1;
#endif
}

bool FVoxelCollisionPresenter::OnTask(FVoxelTaskResult&& InResult)
{
	if (InResult.Kind != EVoxelTaskKind::BuildCollision) return false;
	const auto* Data = Module.GetRuntime()->FindSection(InResult.Stamp.Section);
	auto* Body = FindBody(InResult.Stamp.Section);
	if (!Body || !IsDemanded(InResult.Stamp.Section) || InResult.bCanceled ||
		InResult.Stamp.WorldEpoch != WorldEpoch || InResult.Stamp.OwnerId != OwnerId || !Data ||
		InResult.Stamp != Body->RequestedStamp || InResult.Stamp != CaptureInputStamp(InResult.Stamp.Section)) return true;
	if (!InResult.bSuccess || !InResult.Collision || InResult.Collision->Stamp != InResult.Stamp)
	{
		Body->FailedStamp = InResult.Stamp;
		Body->bHasFailure = true;
		if (!Body->Prepared && !Body->Sync) Control->Deactivate(*Body);
		return true;
	}
	if (Body->PreparingComponent && Body->PreparedStamp != InResult.Stamp)
	{
		Body->PreparingComponent->DestroyComponent();
		Body->PreparingComponent.Reset();
		Body->PreparingLease.Reset();
	}
#if !UE_BUILD_SHIPPING
	if (InResult.Collision->IsEmpty()) ++EmptyBuilds;
	else ++NonemptyBuilds;
#endif
	Body->Prepared = MoveTemp(InResult.Collision);
	Body->bHasFailure = false;
	Body->PreparedStamp = InResult.Stamp;
	Control->Activate(*Body);
	return true;
}

void FVoxelCollisionPresenter::AdvanceBody(const FIntVector& InSection, FBodyRecord& InBody,
	TSharedPtr<FPhysicsSync, ESPMode::ThreadSafe>& InOutEmptySync)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_CollisionAdvanceBody);
	if (InBody.Sync)
	{
		if (!InBody.Sync->bComplete.Load()) return;
		const auto* Data = Module.GetRuntime()->FindSection(InSection);
		const bool bCurrent = !InBody.bRetiring && InBody.Sync->bCommitted.Load() && InBody.SyncPrepared && Data &&
			Data->Status == EVoxelSectionStatus::DataReady && InBody.InstalledStamp == CaptureInputStamp(InSection) &&
			(!InBody.SyncPrepared->FineGeometry || Module.IsOverlayVersionCurrent(InBody.SyncPrepared->FineGeometry->OverlayVersion));
		if (bCurrent)
		{
			InBody.InstalledOverlayVersion = InBody.SyncPrepared->FineGeometry ? InBody.SyncPrepared->FineGeometry->OverlayVersion : nullptr;
			InBody.Receipt = {WorldEpoch, SceneEpoch, InBody.Sync->Step.Load(), InSection,
				InBody.InstalledStamp.Token, InBody.InstalledStamp.Revision,
				InBody.InstalledStamp.GeometryRevision,
				InBody.InstalledStamp.RegistryRevision, InBody.InstalledStamp.ShapeRevision, ++NextBodyGeneration,
				InBody.InstalledStamp.OwnerIncarnation, OwnerId};
			InBody.Receipt.NeighborRevision = InBody.InstalledStamp.NeighborRevision;
			if (InBody.SyncPrepared)
			{
				InBody.InstalledUnknownHaloMask = InBody.SyncPrepared->UnknownHaloMask;
				InBody.RequestedUnknownHaloMask = InBody.InstalledUnknownHaloMask;
				InBody.OcclusionRefreshMask &= InBody.InstalledUnknownHaloMask;
#if !UE_BUILD_SHIPPING
				if (InBody.SyncPrepared->IsEmpty()) ++EmptyReceipts;
				else ++NonemptyReceipts;
#endif
			}
			InBody.bInstalled = true;
			if (InBody.Component) InBody.Component->MarkPhysicsCommitted();
			if (InBody.SyncPrepared && InBody.SyncPrepared->WorkReceipt) InBody.SyncPrepared->WorkReceipt->Resolve(true);
		}
		else InBody.bInstalled = false;
		InstallEvidence(InSection, InBody, !bCurrent);
		InBody.Sync.Reset();
		if (InBody.RetiringComponent) InBody.RetiringComponent->ReleasePreparedInputs(InBody.RetiringLease);
		InBody.RetiringComponent.Reset();
		InBody.RetiringLease.Reset();
		InBody.SyncPrepared.Reset();
		if (InBody.bRetiring)
		{
			if (IsDemanded(InSection))
			{
				// 原生注销回执只结束旧物理体；期间重新出现的Nav需求仍属于该局部服务记录。
				InBody.bRetiring = false;
				InBody.bHasFailure = false;
				InBody.RequestedStamp = {};
				InBody.InstalledStamp = {};
				InBody.Receipt = {};
				InBody.QueuedAt = FPlatformTime::Seconds();
				check(NextBodyQueueSerial < MAX_uint64);
				InBody.QueueSerial = ++NextBodyQueueSerial;
			}
			else RemoveBody(InBody);
		}
		return;
	}
	if (!IsDemanded(InSection)) { BeginRetirement(InBody); return; }
	if (!InBody.Prepared) return;
	const auto* Data = Module.GetRuntime()->FindSection(InSection);
	if (!Data || Data->Status != EVoxelSectionStatus::DataReady || InBody.PreparedStamp != InBody.RequestedStamp ||
		InBody.PreparedStamp != CaptureInputStamp(InSection) ||
		(InBody.Prepared->FineGeometry && !Module.IsOverlayVersionCurrent(InBody.Prepared->FineGeometry->OverlayVersion)))
	{
		if (InBody.PreparingComponent) InBody.PreparingComponent->DestroyComponent();
		InBody.PreparingComponent.Reset(); InBody.Prepared.Reset(); InBody.PreparingLease.Reset();
		return;
	}
	const auto Resources = Scheduler.GetTerrainResources();
	const auto Consumer = FVoxelGenerationConsumerKey::Section(WorldEpoch, OwnerId, Data->Incarnation, InSection, ++NextLeaseSerial);
	if (!InBody.EvidenceLease && !Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Resident,
		33 * EvidenceNodeBytes() + VoxelBoundedAllocation::WorkerSharedBytes<FVoxelCollisionCoverageSnapshot>(), InBody.EvidenceLease, 34)) return;
	if (InBody.Prepared->IsEmpty() && !InBody.PreparingComponent)
	{
		FPhysScene* Scene = Module.GetWorld()->GetPhysicsScene();
		if (!Scene || !Scene->GetSolver()) return;
		const bool bShareEmptySync = !InBody.Component;
		auto NewSync = bShareEmptySync ? InOutEmptySync : nullptr;
		if (!NewSync) NewSync = PreparePhysicsSync(InSection);
		if (!NewSync) return;
		if (bShareEmptySync) InOutEmptySync = NewSync;
		else FPhysicsCommand::ExecuteWrite(Scene, [&]
		{
			InBody.Component->DestroyComponent();
		});
		InBody.RetiringComponent = MoveTemp(InBody.Component);
		InBody.RetiringLease = MoveTemp(InBody.InstalledLease);
		InBody.InstalledStamp = InBody.PreparedStamp;
		InBody.SyncPrepared = MoveTemp(InBody.Prepared);
		InBody.bInstalled = false;
		InBody.bInstalledEmpty = true;
		InBody.Sync = MoveTemp(NewSync);
		Module.NotifyCollisionChanged(InSection);
		const auto Sync = InBody.Sync;
		if (!bShareEmptySync) Scene->GetSolver()->EnqueueCommandImmediate(FPhysicsSyncCommand{Sync, Scene->GetSolver()});
		return;
	}
	if (!InBody.PreparingComponent)
	{
		// 覆盖焊接索引、Chaos 顶点、三角形和构建 BVH 的同时存活峰值。
		const uint64 PhysicalPeak = sizeof(UVoxelCollisionComponent) + sizeof(UBodySetup) + 65536 +
			UVoxelCollisionComponent::PreparedReclaimBytes() +
			uint64(InBody.Prepared->GetVertexCount()) * 128 + uint64(InBody.Prepared->GetIndexCount() / 3) * 256;
		if (!Resources->Acquire(Consumer, InBody.Component ? EVoxelTerrainResourcePool::ResidentOverlap :
			EVoxelTerrainResourcePool::Resident, PhysicalPeak, InBody.PreparingLease, 1)) return;
		AActor* Owner = Module.GetTypedOuter<AActor>();
		if (!Owner) return;
		auto* Component = NewObject<UVoxelCollisionComponent>(Owner);
		InBody.PreparingComponent.Reset(Component);
		Component->SetWorldLocation(FVector(InSection * 16) * Module.BlockSize());
		Component->RegisterComponent();
		if (!Component->BeginPrepareTriangles(InBody.Prepared, Module.BlockSize(), InBody.PreparingLease)) return;
	}
	const auto* CurrentDemand = Module.GetCurrentInterest().Exact.Find(InSection);
	const FVoxelTerrainServicePriority ServicePriority = CurrentDemand &&
		(CurrentDemand->bCollision || CurrentDemand->bWarmupCollision) ? CurrentDemand->ServicePriority :
		FVoxelTerrainServicePriority{EVoxelTerrainServiceBand::P2, 0.0, 0.0, 0, OwnerId};
	const auto Retirement = Module.GetPhysicsRetirement();
	if (!InBody.PreparingComponent->BeginDetachedCook(Scheduler, Consumer, InBody.PreparingLease,
		ServicePriority, InBody.QueuedAt, Retirement) ||
		InBody.PreparingComponent->IsDetachedCookRunning()) return;
	if (InBody.PreparingComponent->HasDetachedCookFailed())
	{
		const bool bCanceled = InBody.PreparingComponent->IsDetachedCookCanceled();
		if (!bCanceled)
		{
			InBody.FailedStamp = InBody.PreparedStamp;
			InBody.bHasFailure = true;
		}
		InBody.PreparingComponent->DestroyComponent();
		InBody.PreparingComponent.Reset();
		InBody.PreparingLease.Reset();
		if (bCanceled)
		{
			InBody.bHasFailure = false;
			UE_LOG(LogTemp, Display, TEXT("Native collision cook canceled; retrying original prepared input: section=%s epoch=%llu token=%llu revision=%llu"),
				*InSection.ToString(), WorldEpoch, InBody.PreparedStamp.Token, InBody.PreparedStamp.Revision);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Native collision cook failed: section=%s epoch=%llu token=%llu revision=%llu"),
				*InSection.ToString(), WorldEpoch, InBody.PreparedStamp.Token, InBody.PreparedStamp.Revision);
			InBody.Prepared.Reset();
		}
		return;
	}
	FPhysScene* Scene = Module.GetWorld()->GetPhysicsScene();
	if (!Scene || !Scene->GetSolver()) return;
	if (!InBody.PreparingComponent->PrepareNativeRetirement(InBody.Prepared->WorkReceipt)) return;
	auto NewSync = PreparePhysicsSync(InSection);
	if (!NewSync) return;
	bool bInstalled = false;
	// 同一个 Chaos Scene 写锁内先验证新 Body，再撤销旧 Body；失败保留旧物理集合。
	FPhysicsCommand::ExecuteWrite(Scene, [&]
	{
		bInstalled = InBody.PreparingComponent->InstallPreparedPhysics();
		if (bInstalled && InBody.Component) InBody.Component->DestroyComponent();
	});
	if (!bInstalled)
	{
		// 场景或接线暂不可用时保留同一候选、票据、旧已安装 Body；下一轮原尝试续行。
		return;
	}
	InBody.RetiringComponent = MoveTemp(InBody.Component);
	InBody.RetiringLease = MoveTemp(InBody.InstalledLease);
	InBody.Component = MoveTemp(InBody.PreparingComponent);
	InBody.InstalledLease = MoveTemp(InBody.PreparingLease);
	InBody.InstalledStamp = InBody.PreparedStamp;
	InBody.SyncPrepared = MoveTemp(InBody.Prepared);
	InBody.bInstalled = false;
	InBody.bInstalledEmpty = false;
	InBody.Sync = MoveTemp(NewSync);
	Module.NotifyCollisionChanged(InSection);
	const auto Sync = InBody.Sync;
	// 同批推送先安装／移除粒子，随后的一次性命令才确认 PT 已消费该版本。
	Scene->GetSolver()->EnqueueCommandImmediate(FPhysicsSyncCommand{Sync, Scene->GetSolver(), InBody.Component->CaptureNativeTicket()});
}

void FVoxelCollisionPresenter::InstallEvidence(const FIntVector& InSection, FBodyRecord& InBody, const bool bInRetired)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_CollisionInstallEvidence);
	check(InBody.EvidenceLease);
	auto Lease = InBody.EvidenceLease->Split(VoxelBoundedAllocation::WorkerSharedBytes<FVoxelCollisionCoverageSnapshot>());
	check(Lease);
	auto Snapshot = VoxelBoundedAllocation::MakeWorkerShared<FVoxelCollisionCoverageSnapshot>(Lease);
	Snapshot->WorldEpoch = WorldEpoch;
	Snapshot->Serial = ++CoverageSerial;
	Snapshot->ResourceLease = MoveTemp(Lease);
	Snapshot->Root = ChangeEvidence(Coverage ? Coverage->Root : nullptr, InSection, 31,
		bInRetired ? TOptional<FVoxelPhysicsReceipt>() : TOptional<FVoxelPhysicsReceipt>(InBody.Receipt), InBody.EvidenceLease);
	Coverage = MoveTemp(Snapshot);
	InBody.EvidenceLease.Reset();
	if (InBody.InstalledLease) InBody.InstalledLease->Transfer(EVoxelTerrainResourcePool::Resident);
	// Ready 只能由实际 PT 回执唤醒；Cook 任务完成和安装发行都还不能启动 Simulation。
	Module.NotifyCollisionChanged(InSection);
}

void FVoxelCollisionPresenter::BeginRetirement(FBodyRecord& InBody)
{
	if (InBody.bRetiring) return;
	InBody.AdmissionRequest.Reset();
	InBody.AdmissionSnapshot.Reset();
	InBody.AdmissionInputLease.Reset();
	InBody.AdmissionOutputLease.Reset();
	InBody.bCapacityBlocked = false;
	const auto Consumer = FVoxelGenerationConsumerKey::Section(WorldEpoch, OwnerId,
		InBody.RequestedStamp.OwnerIncarnation, InBody.Section, ++NextLeaseSerial);
	if (!InBody.EvidenceLease && !Scheduler.GetTerrainResources()->Acquire(Consumer, EVoxelTerrainResourcePool::Resident,
		33 * EvidenceNodeBytes() + VoxelBoundedAllocation::WorkerSharedBytes<FVoxelCollisionCoverageSnapshot>(), InBody.EvidenceLease, 34)) return;
	FPhysScene* Scene = Module.GetWorld()->GetPhysicsScene();
	if (!Scene || !Scene->GetSolver()) return;
	auto NewSync = PreparePhysicsSync(InBody.Section);
	if (!NewSync) return;
	FPhysicsCommand::ExecuteWrite(Scene, [&]
	{
		if (InBody.Component) InBody.Component->DestroyComponent();
		if (InBody.PreparingComponent) InBody.PreparingComponent->DestroyComponent();
	});
	InBody.bInstalled = false;
	InBody.bRetiring = true;
	InBody.RetiringComponent = MoveTemp(InBody.Component);
	InBody.RetiringLease = MoveTemp(InBody.InstalledLease);
	InBody.PreparingComponent.Reset(); InBody.PreparingLease.Reset(); InBody.Prepared.Reset();
	InBody.Sync = MoveTemp(NewSync);
	Control->Activate(InBody);
	Module.NotifyCollisionChanged(InBody.Section);
	const auto Sync = InBody.Sync;
	Scene->GetSolver()->EnqueueCommandImmediate(FPhysicsSyncCommand{Sync, Scene->GetSolver()});
}

void FVoxelCollisionPresenter::RequestCollision(const FIntVector& InSection, const uint64 InRevision,
	const FVoxelExactDemand& InDemand)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_CollisionRequest);
	const auto* Data = Module.GetRuntime()->FindSection(InSection);
	if (!Data || Data->Status != EVoxelSectionStatus::DataReady) return;
	if (!Module.IsAuthority())
	{
		const auto* Replica = Module.GetOverlayReplica();
		uint64 Revision = 0;
		TSharedPtr<const FVoxelOverlaySnapshot, ESPMode::ThreadSafe> Overlay;
		if (!Replica || !Replica->LookupSection(InSection, Revision, Overlay) || Data->CommittedRevision != Revision) return;
		for (uint8 Face = 0; Face < 6; ++Face)
		{
			const FIntVector Neighbor = InSection + VoxelCoord::Direction(Face);
			const auto* NeighborData = Module.GetRuntime()->FindSection(Neighbor);
			if (!NeighborData || NeighborData->Status != EVoxelSectionStatus::DataReady) continue;
			if (!Replica->LookupSection(Neighbor, Revision, Overlay) || NeighborData->CommittedRevision != Revision) return;
		}
	}
	const auto Stamp = CaptureInputStamp(InSection);
	check(Stamp.Revision == InRevision);
	if (Scheduler.Has(Stamp, EVoxelTaskKind::BuildCollision)) return;
	bool bFineGeometryExpected = false;
	const auto* View = Module.GetViewManager();
	const auto FineGeometry = View ? View->FindFineCollisionGeometry(InSection, bFineGeometryExpected) : nullptr;
	if (bFineGeometryExpected && !FineGeometry) return;
	const auto Consumer = FVoxelGenerationConsumerKey::Section(WorldEpoch, OwnerId, Data->Incarnation, InSection, ++NextLeaseSerial);
	const auto Resources = Scheduler.GetTerrainResources();
	FBodyRecord* Body = FindOrAddBody(Consumer);
	if (!Body) return;
	if (Body->RequestedStamp != Stamp)
	{
		Body->AdmissionRequest.Reset();
		Body->AdmissionSnapshot.Reset();
		Body->AdmissionInputLease.Reset();
		Body->AdmissionOutputLease.Reset();
		Body->bCapacityBlocked = false;
		Body->AdmissionConsumer = {};
		Body->RequestedStamp = Stamp;
		Body->bHasFailure = false;
		Body->QueuedAt = FPlatformTime::Seconds();
	}
	if ((Body->Prepared && Body->PreparedStamp == Stamp) ||
		(Body->SyncPrepared && Body->InstalledStamp == Stamp)) return;
	if (FineGeometry)
	{
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
		if (!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Staged,
			VoxelBoundedAllocation::WorkerSharedBytes<FVoxelSectionCollisionResult>(), Lease)) return;
		auto Shared = VoxelBoundedAllocation::MakeWorkerShared<FVoxelSectionCollisionResult>(Lease);
		Shared->ResourceLease = MoveTemp(Lease);
		Shared->Stamp = Stamp;
		if (Shared->BindFineGeometry(FineGeometry, InSection))
		{
			Body->AdmissionRequest.Reset();
			Body->AdmissionSnapshot.Reset();
			Body->AdmissionInputLease.Reset();
			Body->AdmissionOutputLease.Reset();
			Body->AdmissionConsumer = {};
			Body->bCapacityBlocked = false;
			Body->RequestedUnknownHaloMask = 0;
			Body->OcclusionRefreshMask = 0;
			Body->Prepared = MoveTemp(Shared);
			Body->PreparedStamp = Stamp;
			Body->bHasFailure = false;
			Control->Activate(*Body);
#if !UE_BUILD_SHIPPING
			++SharedMeshBuilds;
			if (Body->Prepared->IsEmpty()) ++EmptyBuilds;
			else ++NonemptyBuilds;
#endif
			return;
		}
	}
	if (!Body->AdmissionConsumer.IsValid()) Body->AdmissionConsumer = Consumer;
	if (Body->bCapacityBlocked)
	{
		uint64 Current = 0;
		if (!Resources->TryGetReleaseVersionExcludingOwner(Body->CapacityConsumer, Current) || Current <= Body->CapacityVersion) return;
		Body->bCapacityBlocked = false;
	}
	const uint64 InputBytes = VoxelBoundedAllocation::InitialGrowBytes<uint32>(4096) +
		6 * VoxelBoundedAllocation::InitialGrowBytes<uint32>(256) + VoxelBoundedAllocation::WorkerSharedBytes<FVoxelSectionSnapshot>();
	const uint64 OutputBytes = FVoxelCollisionBuilder::MaximumOutputBytes();
	if (!Body->AdmissionSnapshot)
	{
		const uint64 BeforeInput = Resources->ReleaseVersionExcludingOwner(Body->AdmissionConsumer);
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InputLease, OutputLease;
		auto Admission = Resources->TryAcquire(Body->AdmissionConsumer, EVoxelTerrainResourcePool::Input, InputBytes, InputLease);
		if (Admission == EVoxelTerrainResourceAdmission::Granted)
		{
			Admission = Resources->TryAcquire(Body->AdmissionConsumer, EVoxelTerrainResourcePool::Staged,
				OutputBytes, OutputLease, 0, InputLease->Bytes(), 1);
		}
		if (Admission != EVoxelTerrainResourceAdmission::Granted)
		{
			if (Admission == EVoxelTerrainResourceAdmission::CapacityBlocked)
			{
				Body->bCapacityBlocked = true;
				Body->CapacityConsumer = Body->AdmissionConsumer;
				Body->CapacityVersion = BeforeInput;
			}
			else
			{
				Body->FailedStamp = Stamp;
				Body->bHasFailure = true;
			}
			return;
		}
		auto Snapshot = VoxelBoundedAllocation::MakeWorkerShared<FVoxelSectionSnapshot>(InputLease);
		if (!Module.GetRuntime()->CaptureSnapshot(InSection, *Snapshot, InputLease)) return;
		Body->RequestedUnknownHaloMask = 0;
		for (int32 Face = 0; Face < 6; ++Face)
		{
			if (!Snapshot->Known[Face]) Body->RequestedUnknownHaloMask |= uint8(1u << Face);
		}
		// 邻区已再次卸载时不循环重建；下次真实就绪通知会重新唤醒。
		Body->OcclusionRefreshMask &= uint8(~Body->RequestedUnknownHaloMask);
		Body->AdmissionSnapshot = MoveTemp(Snapshot);
		Body->AdmissionInputLease = MoveTemp(InputLease);
		Body->AdmissionOutputLease = MoveTemp(OutputLease);
		Body->AdmissionRequest.Emplace();
		auto& Request = Body->AdmissionRequest.GetValue();
		Request.Kind = EVoxelTaskKind::BuildCollision;
		Request.Stamp = Stamp;
		Request.ServiceBand = InDemand.ServicePriority.Band;
		Request.SourceStableKey = InDemand.ServicePriority.SourceStableKey;
		Request.SourcePriority = InDemand.ServicePriority.SourcePriority;
		Request.DistanceScore = InDemand.ServicePriority.DistanceCells;
		Request.ForwardScore = InDemand.ServicePriority.ForwardScore;
		Request.WorkClass = FVoxelTerrainWorkSelection::IsNear(Request.ServiceBand) ? EVoxelWorkClass::Critical :
			Request.ServiceBand == EVoxelTerrainServiceBand::P6 ? EVoxelWorkClass::Background : EVoxelWorkClass::Interactive;
		Request.FirstRunnableSeconds = Body->QueuedAt;
		Request.InputBytes = InputBytes;
		Request.SetRequiredBindingInput(Body->AdmissionInputLease->Bytes(), 2);
		Request.ReservedBytes = OutputBytes;
	}
	auto& Request = Body->AdmissionRequest.GetValue();
	const auto Snapshot = Body->AdmissionSnapshot;
	const auto InputLease = Body->AdmissionInputLease;
	const auto OutputLease = Body->AdmissionOutputLease;
	auto Execute = [Snapshot, Registry = Module.GetRegistry(), Shapes = Module.GetShapes(), InputLease, OutputLease, Stamp]
		(const TAtomic<bool>& InCancel)
	{
		FVoxelTaskResult Result;
		Result.Collision = VoxelBoundedAllocation::MakeWorkerShared<FVoxelSectionCollisionResult>(OutputLease);
		Result.Collision->ResourceLease = OutputLease;
		Result.Collision->Stamp = Stamp;
		Result.bSuccess = Registry && Shapes && FVoxelCollisionBuilder::Build(*Snapshot, *Registry, *Shapes, *Result.Collision, &InCancel);
		return Result;
	};
	FString BindingError;
	const auto BindingConsumer = Scheduler.GetControlConsumer();
	const uint64 BeforeBinding = Resources->ReleaseVersionExcludingOwner(BindingConsumer);
	const bool bBound = Request.bExecuteBound || Request.BindExecute(Resources, BindingConsumer, MoveTemp(Execute), BindingError);
	if (bBound)
	{
		FVoxelTaskRequest AdmissionRequest = MoveTemp(Request);
		Body->AdmissionRequest.Reset();
		const bool bAccepted = Scheduler.Enqueue(MoveTemp(AdmissionRequest));
		Body = FindBody(InSection);
		if (!Body || Body->RequestedStamp != Stamp || Body->AdmissionSnapshot != Snapshot) return;
		if (bAccepted)
		{
			Body->AdmissionSnapshot.Reset();
			Body->AdmissionInputLease.Reset();
			Body->AdmissionOutputLease.Reset();
			return;
		}
		Body->AdmissionRequest.Emplace(MoveTemp(AdmissionRequest));
	}
	const auto Reason = Body->AdmissionRequest->Blocked;
	if (BindingError.StartsWith(TEXT("CapacityBlocked")) || (bBound &&
		(Reason == EVoxelTaskBlockReason::InputMemory || Reason == EVoxelTaskBlockReason::Memory)))
	{
		Body->bCapacityBlocked = true;
		Body->CapacityConsumer = BindingConsumer;
		Body->CapacityVersion = BeforeBinding;
	}
	else if (!bBound || Reason == EVoxelTaskBlockReason::Oversized || Reason == EVoxelTaskBlockReason::TerminalFailure)
	{
		Body->FailedStamp = Stamp;
		Body->bHasFailure = true;
		Body->AdmissionRequest.Reset();
		Body->AdmissionSnapshot.Reset();
		Body->AdmissionInputLease.Reset();
		Body->AdmissionOutputLease.Reset();
	}
}

void FVoxelCollisionPresenter::InvalidateSection(const FIntVector& InSection)
{
	if (auto* Body = FindBody(InSection))
	{
		Body->bHasFailure = false;
		Body->AdmissionRequest.Reset();
		Body->AdmissionSnapshot.Reset();
		Body->AdmissionInputLease.Reset();
		Body->AdmissionOutputLease.Reset();
		Body->AdmissionConsumer = {};
		Body->bCapacityBlocked = false;
		Control->Activate(*Body);
	}
	Scheduler.CancelMatching([&InSection](const EVoxelTaskKind Kind, const FVoxelTaskStamp& Stamp)
	{
		return Kind == EVoxelTaskKind::BuildCollision && Stamp.Section == InSection;
	});
}

void FVoxelCollisionPresenter::InvalidateNeighbors(const FIntVector& InSection)
{
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		for (const int32 Direction : {-1, 1})
		{
			const int64 Coordinate = int64(InSection[Axis]) + Direction;
			if (Coordinate < MIN_int32 || Coordinate > MAX_int32) continue;
			FIntVector Neighbor = InSection;
			Neighbor[Axis] = int32(Coordinate);
			if (!FindBody(Neighbor)) continue;
			InvalidateSection(Neighbor);
			Module.NotifyCollisionChanged(Neighbor);
		}
	}
}

void FVoxelCollisionPresenter::NotifySectionDataReady(const FIntVector& InSection)
{
	QueueReadySection(InSection);
	if (!Control) return;
	for (int32 Face = 0; Face < 6; ++Face)
	{
		const int32 Axis = Face / 2;
		const int32 Direction = (Face & 1) ? -1 : 1;
		const int64 Coordinate = int64(InSection[Axis]) - Direction;
		if (Coordinate < MIN_int32 || Coordinate > MAX_int32) continue;
		FIntVector Neighbor = InSection;
		Neighbor[Axis] = int32(Coordinate);
		auto* Body = FindBody(Neighbor);
		const uint8 Mask = uint8(1u << Face);
		if (!Body || !((Body->RequestedUnknownHaloMask | Body->InstalledUnknownHaloMask) & Mask)) continue;
		Body->OcclusionRefreshMask |= Mask;
		Control->Activate(*Body);
	}
}

void FVoxelCollisionPresenter::NotifyFineGeometryReady(const FVoxelFineShardKey& InKey)
{
	const int32 Side = InKey.SideCells / VoxelBlock::Size;
	if (Side <= 0 || Side > 4 || InKey.SideCells % VoxelBlock::Size != 0) return;
	const FIntVector First = InKey.Min / VoxelBlock::Size;
	for (int32 Z = 0; Z < Side; ++Z)
		for (int32 Y = 0; Y < Side; ++Y)
			for (int32 X = 0; X < Side; ++X)
				QueueReadySection(First + FIntVector(X, Y, Z));
}

void FVoxelCollisionPresenter::QueueReadySection(const FIntVector& InSection)
{
	const auto* Data = Module.GetRuntime()->FindSection(InSection);
	if (!Data || Data->Status != EVoxelSectionStatus::DataReady || !IsDemanded(InSection)) return;
	if (const auto* Existing = FindBody(InSection); IsReady(InSection, Data, Existing) && Existing->OcclusionRefreshMask == 0) return;
	if (const auto* View = Module.GetViewManager())
	{
		bool bExpected = false;
		const auto Geometry = View->FindFineCollisionGeometry(InSection, bExpected);
		if (bExpected && !Geometry) return;
	}
	const auto Consumer = FVoxelGenerationConsumerKey::Section(WorldEpoch, OwnerId, Data->Incarnation, InSection, ++NextLeaseSerial);
	if (auto* Body = FindOrAddBody(Consumer))
	{
		if (Body->RequestedStamp.OwnerIncarnation == 0) Body->RequestedStamp = CaptureInputStamp(InSection);
		Control->Activate(*Body);
	}
}

bool FVoxelCollisionPresenter::IsReady(const FIntVector& InSection) const
{
	const auto* Data = Module.GetRuntime() ? Module.GetRuntime()->FindSection(InSection) : nullptr;
	const auto* Body = FindBody(InSection);
	return IsReady(InSection, Data, Body);
}

bool FVoxelCollisionPresenter::HasCurrentCollisionFailure(const FIntVector& InSection) const
{
	check(IsInGameThread());
	const FVoxelWorldRuntime* Runtime = Module.GetRuntime();
	if (!Runtime || Runtime->Epoch() != WorldEpoch || Module.GetWorldEpoch() != WorldEpoch ||
		!Module.GetRegistry() || !Module.GetShapes() || !IsDemanded(InSection))
	{
		return false;
	}
	const FVoxelSection* Data = Runtime->FindSection(InSection);
	const FBodyRecord* Body = FindBody(InSection);
	if (!Data || Data->Status != EVoxelSectionStatus::DataReady || !Body || !Body->bHasFailure ||
		Body->bRetiring || Body->bCapacityBlocked || IsReady(InSection, Data, Body))
	{
		return false;
	}
	const FVoxelTaskStamp CurrentStamp = CaptureInputStamp(InSection);
	if (Body->RequestedStamp != CurrentStamp || Body->FailedStamp != CurrentStamp)
	{
		return false;
	}
	if (Scheduler.Has(CurrentStamp, EVoxelTaskKind::BuildCollision) ||
		(Body->Prepared && Body->PreparedStamp == CurrentStamp) ||
		(Body->Sync && Body->InstalledStamp == CurrentStamp))
	{
		return false;
	}
	return true;
}

bool FVoxelCollisionPresenter::IsReadyEmpty(const FIntVector& InSection) const
{
	const auto* Data = Module.GetRuntime() ? Module.GetRuntime()->FindSection(InSection) : nullptr;
	const auto* Body = FindBody(InSection);
	return IsReady(InSection, Data, Body) && Body->bInstalledEmpty;
}

bool FVoxelCollisionPresenter::IsReady(const FIntVector& InSection, const FVoxelSection* InData, const FBodyRecord* InBody) const
{
	if (!InData || InData->Status != EVoxelSectionStatus::DataReady || !InBody || !InBody->bInstalled ||
		(InBody->InstalledOverlayVersion && !Module.IsOverlayVersionCurrent(InBody->InstalledOverlayVersion)) ||
		!((InBody->bInstalledEmpty && !InBody->Component) || (InBody->Component && InBody->Component->HasInstalledPhysics()))) return false;
	// 同一个 PT 回执同步写入 Body 和不可变 Coverage；热路径无需再次遍历覆盖树。
	return InBody->Receipt.MatchesInput(CaptureInputStamp(InSection), SceneEpoch);
}

TSharedPtr<const FVoxelCollisionCoverageSnapshot, ESPMode::ThreadSafe> FVoxelCollisionPresenter::CaptureSnapshot() const
{
	return Coverage;
}

bool FVoxelCollisionPresenter::CaptureCurrentReceipt(const FIntVector& InSection,
	FVoxelPhysicsReceipt& OutReceipt) const
{
	if (!IsReady(InSection)) return false;
	const auto* Receipt = Coverage ? Coverage->Find(InSection) : nullptr;
	if (!Receipt || !Receipt->MatchesInput(CaptureInputStamp(InSection), SceneEpoch)) return false;
	OutReceipt = *Receipt;
	return true;
}

void FVoxelCollisionPresenter::Reset()
{
	check(IsInGameThread());
	// 明确 StopWorld 的解绑必须先完成，随后新世界才可在同一 Scene 安装。
	// 原生 Actor/Setup/物理租约由既有 NativeTicket 等真实销毁证明，不依赖这里的控制记录。
	for (auto* Body = Control ? Control->Index.GetHead() : nullptr; Body; Body = Body->Next)
	{
		if (Body->Component) Body->Component->DestroyComponent();
		if (Body->PreparingComponent) Body->PreparingComponent->DestroyComponent();
		if (Body->RetiringComponent) Body->RetiringComponent->DestroyComponent();
		Body->Component.Reset();
		Body->PreparingComponent.Reset();
		Body->RetiringComponent.Reset();
	}
	// 根的删除任务在创建时已授予；全部控制 backing、结果和证据树在 Worker 释放。
	Control.Reset();
	Coverage.Reset();
	DemandCursor = 0;
	CriticalDemandCursor = 0;
#if !UE_BUILD_SHIPPING
	NextActiveDiagnostic = 0.0;
	EmptyBuilds = NonemptyBuilds = SharedMeshBuilds = EmptyReceipts = NonemptyReceipts = 0;
#endif
	Selection.Reset();
}

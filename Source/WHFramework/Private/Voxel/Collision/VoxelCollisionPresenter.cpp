#include "Voxel/Collision/VoxelCollisionPresenter.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Physics/PhysicsInterfaceCore.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/BoxElem.h"
#include "PBDRigidsSolver.h"
#include "Chaos/SimCallbackObject.h"
#include "Voxel/Components/VoxelCollisionComponent.h"
#include "Voxel/Collision/VoxelPhysicsRetirement.h"
#include "Voxel/Collision/VoxelCollisionBodyIndex.h"
#include "Voxel/Geometry/VoxelCollisionBuilder.h"
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
	return true;
}

bool FVoxelPhysicsReceipt::MatchesInput(const FVoxelTaskStamp& InInput, const FGuid& InSceneEpoch) const
{
	return WorldEpoch != 0 && PresenterOwnerId.IsValid() && SceneEpoch.IsValid() && SectionIncarnation != 0 &&
		WorldEpoch == InInput.WorldEpoch && PresenterOwnerId == InInput.OwnerId &&
		SceneEpoch == InSceneEpoch && PhysicsStep != INDEX_NONE && BodyGeneration != 0 &&
		Section == InInput.Section && SectionToken == InInput.Token && SectionIncarnation == InInput.OwnerIncarnation &&
		DataRevision == InInput.Revision && ContentRevision == InInput.GeometryRevision &&
		RegistryRevision == InInput.RegistryRevision && ShapeRevision == InInput.ShapeRevision;
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
	const auto Snapshot = Module.GetCurrentInterestSnapshot();
	if (!Snapshot || !Snapshot->PublicationInputs) return;
	const auto& Keys = Snapshot->PublicationInputs->RuntimeSections;
	const double Deadline = Module.GetTerrainFrameContext().Deadline(0.5);
	struct FOperation { FIntVector Section; bool bBody = false; };
	for (int32 Work = 0; Work < 16 && FPlatformTime::Seconds() < Deadline; ++Work)
	{
		TArray<FVoxelTerrainServiceCandidate, TInlineAllocator<128>> Candidates;
		TArray<FOperation, TInlineAllocator<128>> Operations;
		const double Now = FPlatformTime::Seconds();
		for (int32 Scan = 0; Scan < FMath::Min(64, Keys.Num()); ++Scan)
		{
			DemandCursor %= Keys.Num();
			const FIntVector Key = Keys[DemandCursor++];
			const auto* Demand = InDemand.Find(Key);
			const auto* Data = Module.GetRuntime()->FindSection(Key);
			const FBodyRecord* Body = FindBody(Key);
			if (!Demand || (!Demand->bCollision && !Demand->bWarmupCollision) || !Data ||
				Data->Status != EVoxelSectionStatus::DataReady || IsReady(Key) || (Body && (Body->Prepared || Body->Sync))) continue;
			const auto Stamp = CaptureInputStamp(Key);
			if (Scheduler.Has(Stamp, EVoxelTaskKind::BuildCollision) ||
				(Body && Body->bHasFailure && Body->FailedStamp == Stamp)) continue;
			auto& Candidate = Candidates.AddDefaulted_GetRef();
			Candidate.Index = Operations.Add({Key, false});
			Candidate.Priority = Demand->ServicePriority;
			Candidate.FirstQueuedSeconds = Body && Body->RequestedStamp == Stamp ? Body->QueuedAt : Now;
			Candidate.Sequence = Data->Stamp.Token;
			Candidate.EstimatedMicroseconds = StepCostMicroseconds;
			Candidate.bRunnable = true;
		}
		for (int32 Scan = 0; Control && Scan < FMath::Min(32, Control->Index.Num()); ++Scan)
		{
			const auto* Body = Control->Index.NextForScan();
			check(Body);
			const FIntVector Key = Body->Section;
			if (!Body || (Body->Sync && !Body->Sync->bComplete.Load()) ||
				(Body->PreparingComponent && Body->PreparingComponent->IsDetachedCookRunning())) continue;
			const bool bGenerate = !Body->Prepared && !Body->Sync && !Body->bRetiring && IsDemanded(Key);
			if (bGenerate)
			{
				const auto* Data = Module.GetRuntime()->FindSection(Key);
				if (!Data || Data->Status != EVoxelSectionStatus::DataReady || IsReady(Key)) continue;
				const auto Stamp = CaptureInputStamp(Key);
				if (Scheduler.Has(Stamp, EVoxelTaskKind::BuildCollision) ||
					(Body->bHasFailure && Body->FailedStamp == Stamp)) continue;
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
		}
		FVoxelTerrainServiceCapacity Capacity;
		Capacity.bAllowOverdueOpportunity = Module.GetTerrainFrameContext().IsOverdueOpportunityAvailable();
		FVoxelTerrainServiceCharge Charge;
		const int32 Chosen = Selection.Select(Candidates, Capacity, Now, Charge);
		if (Chosen == INDEX_NONE) return;
		if (FPlatformTime::Seconds() >= Deadline) { Selection.RefundBeforeStart(Charge.Serial); return; }
		if (Charge.bOverdueOpportunity) Module.GetTerrainFrameContext().ConsumeOverdueOpportunity();
		const double Start = FPlatformTime::Seconds();
		const auto Operation = Operations[Chosen];
		if (Operation.bBody)
		{
			if (auto* Body = FindBody(Operation.Section)) AdvanceBody(Operation.Section, *Body);
		}
		else if (const auto* Data = Module.GetRuntime()->FindSection(Operation.Section))
		{
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
		return true;
	}
	if (Body->PreparingComponent && Body->PreparedStamp != InResult.Stamp)
	{
		Body->PreparingComponent->DestroyComponent();
		Body->PreparingComponent.Reset();
		Body->PreparingLease.Reset();
	}
	Body->Prepared = MoveTemp(InResult.Collision);
	Body->bHasFailure = false;
	Body->PreparedStamp = InResult.Stamp;
	Body->BoxCursor = 0;
	return true;
}

void FVoxelCollisionPresenter::AdvanceBody(const FIntVector& InSection, FBodyRecord& InBody)
{
	if (InBody.Sync)
	{
		if (!InBody.Sync->bComplete.Load()) return;
		if (!InBody.bRetiring && InBody.Sync->bCommitted.Load())
		{
			InBody.Receipt = {WorldEpoch, SceneEpoch, InBody.Sync->Step.Load(), InSection,
				InBody.InstalledStamp.Token, InBody.InstalledStamp.Revision,
				InBody.InstalledStamp.GeometryRevision,
				InBody.InstalledStamp.RegistryRevision, InBody.InstalledStamp.ShapeRevision, ++NextBodyGeneration,
				InBody.InstalledStamp.OwnerIncarnation, OwnerId};
			InBody.bInstalled = true;
			if (InBody.Component) InBody.Component->MarkPhysicsCommitted();
			if (InBody.SyncPrepared && InBody.SyncPrepared->WorkReceipt) InBody.SyncPrepared->WorkReceipt->Resolve(true);
		}
		else InBody.bInstalled = false;
		InstallEvidence(InSection, InBody, InBody.bRetiring || !InBody.Sync->bCommitted.Load());
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
	if (!Data || InBody.PreparedStamp != InBody.RequestedStamp || InBody.PreparedStamp != CaptureInputStamp(InSection))
	{
		if (InBody.PreparingComponent) InBody.PreparingComponent->DestroyComponent();
		InBody.PreparingComponent.Reset(); InBody.Prepared.Reset(); InBody.PreparingLease.Reset();
		return;
	}
	const auto Resources = Scheduler.GetTerrainResources();
	const auto Consumer = FVoxelGenerationConsumerKey::Section(WorldEpoch, OwnerId, Data->Incarnation, InSection, ++NextLeaseSerial);
	if (!InBody.EvidenceLease && !Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Resident,
		33 * EvidenceNodeBytes() + VoxelBoundedAllocation::WorkerSharedBytes<FVoxelCollisionCoverageSnapshot>(), InBody.EvidenceLease, 34)) return;
	if (!InBody.PreparingComponent)
	{
		const uint64 PhysicalPeak = sizeof(UVoxelCollisionComponent) + sizeof(UBodySetup) + 4096 +
			UVoxelCollisionComponent::PreparedReclaimBytes() +
			VoxelBoundedAllocation::ArrayBytes<FKBoxElem>(InBody.Prepared->Boxes.Num()) +
			uint64(InBody.Prepared->Boxes.Num()) * 1024;
		if (!Resources->Acquire(Consumer, InBody.Component ? EVoxelTerrainResourcePool::ResidentOverlap :
			EVoxelTerrainResourcePool::Resident, PhysicalPeak, InBody.PreparingLease, 1)) return;
		AActor* Owner = Module.GetTypedOuter<AActor>();
		if (!Owner) return;
		auto* Component = NewObject<UVoxelCollisionComponent>(Owner);
		InBody.PreparingComponent.Reset(Component);
		Component->SetWorldLocation(FVector(InSection * 16) * Module.BlockSize());
		Component->RegisterComponent();
		if (!Component->BeginPrepare(InBody.Prepared->Boxes.Num(), Module.BlockSize(), InBody.PreparingLease)) return;
	}
	if (InBody.BoxCursor < InBody.Prepared->Boxes.Num())
	{
		const int32 Count = FMath::Min(64, InBody.Prepared->Boxes.Num() - InBody.BoxCursor);
		if (!InBody.PreparingComponent->AppendPreparedBoxes(MakeArrayView(InBody.Prepared->Boxes.GetData() + InBody.BoxCursor, Count))) return;
		InBody.BoxCursor += Count;
		return;
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
		InBody.FailedStamp = InBody.PreparedStamp;
		InBody.bHasFailure = true;
		InBody.PreparingComponent->DestroyComponent();
		InBody.PreparingComponent.Reset();
		InBody.PreparingLease.Reset();
		InBody.Prepared.Reset();
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
	InBody.Sync = MoveTemp(NewSync);
	Module.NotifyCollisionChanged(InSection);
	const auto Sync = InBody.Sync;
	// 同批推送先安装／移除粒子，随后的一次性命令才确认 PT 已消费该版本。
	Scene->GetSolver()->EnqueueCommandImmediate(FPhysicsSyncCommand{Sync, Scene->GetSolver(), InBody.Component->CaptureNativeTicket()});
}

void FVoxelCollisionPresenter::InstallEvidence(const FIntVector& InSection, FBodyRecord& InBody, const bool bInRetired)
{
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
	Module.NotifyCollisionChanged(InBody.Section);
	const auto Sync = InBody.Sync;
	Scene->GetSolver()->EnqueueCommandImmediate(FPhysicsSyncCommand{Sync, Scene->GetSolver()});
}

void FVoxelCollisionPresenter::RequestCollision(const FIntVector& InSection, const uint64 InRevision,
	const FVoxelExactDemand& InDemand)
{
	const auto* Data = Module.GetRuntime()->FindSection(InSection);
	if (!Data || Data->Status != EVoxelSectionStatus::DataReady) return;
	const auto Stamp = CaptureInputStamp(InSection);
	check(Stamp.Revision == InRevision);
	if (Scheduler.Has(Stamp, EVoxelTaskKind::BuildCollision)) return;
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
	if (!Body->AdmissionConsumer.IsValid()) Body->AdmissionConsumer = Consumer;
	if (Body->bCapacityBlocked)
	{
		uint64 Current = 0;
		if (!Resources->TryGetReleaseVersionExcludingOwner(Body->CapacityConsumer, Current) || Current <= Body->CapacityVersion) return;
		Body->bCapacityBlocked = false;
	}
	const uint64 InputBytes = VoxelBoundedAllocation::InitialGrowBytes<uint32>(4096) +
		6 * VoxelBoundedAllocation::InitialGrowBytes<uint32>(256) + VoxelBoundedAllocation::WorkerSharedBytes<FVoxelSectionSnapshot>();
	const uint64 OutputBytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelSectionCollisionResult>() +
		VoxelBoundedAllocation::ArrayBytes<FBox>(16384);
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
		Request.WorkClass = FVoxelTerrainWorkSelection::IsNear(Request.ServiceBand) ? EVoxelWorkClass::Critical : EVoxelWorkClass::Interactive;
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
	}
	Scheduler.CancelMatching([&InSection](const EVoxelTaskKind Kind, const FVoxelTaskStamp& Stamp)
	{
		return Kind == EVoxelTaskKind::BuildCollision && Stamp.Section == InSection;
	});
}

bool FVoxelCollisionPresenter::IsReady(const FIntVector& InSection) const
{
	const auto* Data = Module.GetRuntime() ? Module.GetRuntime()->FindSection(InSection) : nullptr;
	const auto* Body = FindBody(InSection);
	const auto* Receipt = Coverage ? Coverage->Find(InSection) : nullptr;
	return Data && Data->Status == EVoxelSectionStatus::DataReady && Body && Body->bInstalled &&
		Body->Component && Body->Component->HasInstalledPhysics() &&
		Receipt && Receipt->MatchesInput(CaptureInputStamp(InSection), SceneEpoch);
}

TSharedPtr<const FVoxelCollisionCoverageSnapshot, ESPMode::ThreadSafe> FVoxelCollisionPresenter::CaptureSnapshot() const
{
	return Coverage;
}

bool FVoxelCollisionPresenter::CaptureCurrentReceipt(const FIntVector& InSection,
	FVoxelPhysicsReceipt& OutReceipt) const
{
	if (!IsReady(InSection)) return false;
	OutReceipt = *Coverage->Find(InSection);
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
	Selection.Reset();
}

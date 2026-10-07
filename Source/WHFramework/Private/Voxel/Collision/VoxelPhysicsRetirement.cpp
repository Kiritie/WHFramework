#include "Voxel/Collision/VoxelPhysicsRetirement.h"

#include "Chaos/SimCallbackObject.h"
#include "Containers/Ticker.h"
#include "HAL/CriticalSection.h"
#include "Misc/ScopeLock.h"
#include "PBDRigidsSolver.h"
#include "Chaos/PBDRigidsEvolutionGBF.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/Task/VoxelTerrainResources.h"
#include "Voxel/Rendering/VoxelTerrainFrameContext.h"
#include "Voxel/Rendering/VoxelTerrainStatistics.h"

struct FVoxelPhysicsRetirementNode
{
	TSharedPtr<FVoxelPhysicsRetirementNode, ESPMode::ThreadSafe> Children[16];
	TSharedPtr<FVoxelPhysicsRetirementTicket, ESPMode::ThreadSafe> Ticket;
};

namespace
{
	struct FPhysicsPayloadReclaimTask
	{
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> MetadataLease;
		TSharedPtr<FVoxelPhysicsRetirementPayload, ESPMode::ThreadSafe> Payload;
		void operator()() const {}
	};

	uint8 PhysicsBranch(const uint64 InKey, const int32 InDepth)
	{
		return uint8((InKey >> (60 - 4 * InDepth)) & 15);
	}
}

bool FVoxelNativeRetirementFence::ObserveDestruction(const uint64 InOrdinal)
{
	if (bObservedDestruction) return false;
	DestructionOrdinal = InOrdinal;
	bObservedDestruction = true;
	return true;
}

bool FVoxelNativeRetirementFence::IsPastDestruction(const uint64 InOrdinal) const
{
	const uint64 Distance = InOrdinal - DestructionOrdinal;
	return bObservedDestruction && Distance != 0 && Distance < (1ull << 63);
}

bool FVoxelPhysicsRetirementTicket::ConfirmPhysicsCommitted(Chaos::FPBDRigidsSolver& InSolver)
{
	// 仅由同一 Solver 的 PT 命令调用。销毁事件之前 Proxy 存活，之后禁止解引用。
	if (!Actor || bNativeRetired.Load() || Fence.bObservedDestruction || Actor->GetSolverBase() != &InSolver) return false;
	const auto* Handle = Actor->GetHandle_LowLevel();
	const auto* Evolution = InSolver.GetEvolution();
	if (!Handle || !Evolution || !Handle->IsInAnyList(
		Evolution->GetParticles().GetNonDisabledStaticParticles().GetContainerListMask())) return false;
	bPhysicsCommitted.Store(true);
	return true;
}

struct FVoxelPhysicsRetirement::FState
{
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> MetadataLease;
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	TSharedPtr<FVoxelPhysicsRetirementNode, ESPMode::ThreadSafe> Root;
	FCriticalSection RootMutex;
	Chaos::FPBDRigidsSolver* Solver = nullptr;
	Chaos::ISimCallbackObject* Callback = nullptr;
	TAtomic<bool> bCallbackDestroyed{false};
	TAtomic<FVoxelPhysicsRetirementTicket*> ReadyHead{nullptr};
	FVoxelPhysicsRetirementTicket* ReadyBatch = nullptr;
	FVoxelPhysicsRetirementTicket* ActiveHead = nullptr;
	FVoxelPhysicsRetirementTicket* ClosingCursor = nullptr;
	FVoxelPhysicsRetirementTicket* PendingHead = nullptr;
	FVoxelPhysicsRetirementTicket* PendingTail = nullptr;
	uint64 PhysicsOrdinal = 0;
	bool bClosing = false;
	bool bUnregisterRequested = false;
	double RetirementCostMilliseconds = 0.01;

	void PushReady(FVoxelPhysicsRetirementTicket& InTicket)
	{
		auto* Head = ReadyHead.Load();
		do { InTicket.ReadyNext = Head; } while (!ReadyHead.CompareExchange(Head, &InTicket));
	}

	void ObserveDestruction(const Chaos::FPhysicsObject* InObject)
	{
		TSharedPtr<FVoxelPhysicsRetirementTicket, ESPMode::ThreadSafe> Ticket;
		{
			// GT 只做固定路径的指针修改；锁内没有分配、析构或原生 Actor 操作。
			FScopeLock Lock(&RootMutex);
			auto* Node = Root.Get();
			const uint64 Key = uint64(reinterpret_cast<UPTRINT>(InObject));
			for (int32 Depth = 0; Depth < 16 && Node; ++Depth) Node = Node->Children[PhysicsBranch(Key, Depth)].Get();
			if (Node) Ticket = Node->Ticket;
		}
		if (!Ticket || Ticket->bNativeRetired.Load() || !Ticket->Fence.ObserveDestruction(PhysicsOrdinal)) return;
		Ticket->PendingNext = nullptr;
		if (PendingTail) PendingTail->PendingNext = Ticket.Get();
		else PendingHead = Ticket.Get();
		PendingTail = Ticket.Get();
	}

	void AdvancePhysics()
	{
		++PhysicsOrdinal;
		for (int32 Count = 0; Count < 64 && PendingHead; ++Count)
		{
			auto* Ticket = PendingHead;
			if (!Ticket->Fence.IsPastDestruction(PhysicsOrdinal)) break;
			PendingHead = Ticket->PendingNext;
			if (!PendingHead) PendingTail = nullptr;
			Ticket->PendingNext = nullptr;
			Ticket->bNativeRetired.Store(true);
			PushReady(*Ticket);
		}
	}

	bool DetachTicket(FVoxelPhysicsRetirementTicket& InTicket)
	{
		TSharedPtr<FVoxelPhysicsRetirementNode, ESPMode::ThreadSafe> RetiredNodes[17];
		TSharedPtr<FVoxelPhysicsRetirementTicket, ESPMode::ThreadSafe> Owner;
		if (!RootMutex.TryLock()) return false;
		TSharedPtr<FVoxelPhysicsRetirementNode, ESPMode::ThreadSafe>* Links[17];
		Links[0] = &Root;
		for (int32 Depth = 0; Depth < 16; ++Depth)
		{
			check(*Links[Depth]);
			Links[Depth + 1] = &(*Links[Depth])->Children[PhysicsBranch(InTicket.PhysicsObjectKey, Depth)];
		}
		check(*Links[16] && (*Links[16])->Ticket.Get() == &InTicket);
		Owner = MoveTemp((*Links[16])->Ticket);
		for (int32 Depth = 16; Depth >= 0; --Depth)
		{
			bool bEmpty = !(*Links[Depth])->Ticket;
			for (const auto& Child : (*Links[Depth])->Children) bEmpty &= !Child;
			if (!bEmpty) break;
			RetiredNodes[Depth] = MoveTemp(*Links[Depth]);
		}
		RootMutex.Unlock();
		if (InTicket.ActivePrevious) InTicket.ActivePrevious->ActiveNext = InTicket.ActiveNext;
		else ActiveHead = InTicket.ActiveNext;
		if (InTicket.ActiveNext) InTicket.ActiveNext->ActivePrevious = InTicket.ActivePrevious;
		if (ClosingCursor == &InTicket) ClosingCursor = InTicket.ActiveNext ? InTicket.ActiveNext : ActiveHead;
		InTicket.ActivePrevious = InTicket.ActiveNext = nullptr;
		InTicket.bRegistered = false;
		if (InTicket.bPhysicsCommitted.Load())
		{
			if (InTicket.CookWorkReceipt) InTicket.CookWorkReceipt->Resolve(true);
			if (InTicket.InputWorkReceipt) InTicket.InputWorkReceipt->Resolve(true);
		}
		UE::Tasks::Launch(UE_SOURCE_LOCATION, FPhysicsPayloadReclaimTask{InTicket.MetadataLease, MoveTemp(InTicket.Payload)});
		return true;
	}

	bool Tick(FVoxelTerrainFrameContext* InFrame = nullptr)
	{
		check(IsInGameThread());
		const double Deadline = InFrame ? InFrame->Deadline(0.25) : FPlatformTime::Seconds() + 0.00025;
		const bool bSolverGone = bCallbackDestroyed.Load();
		if (bSolverGone)
		{
			// Callback 在 Solver 基类析构中释放，此时派生 Evolution/原生对象已结束。
			ReadyBatch = nullptr;
			ReadyHead.Exchange(nullptr);
		}
		for (int32 Count = 0; Count < 32 && FPlatformTime::Seconds() < Deadline; ++Count)
		{
			if (!ReadyBatch && !bSolverGone) ReadyBatch = ReadyHead.Exchange(nullptr);
			if (bSolverGone && !ClosingCursor) ClosingCursor = ActiveHead;
			auto* Ticket = bSolverGone ? ClosingCursor : ReadyBatch;
			if (!Ticket) break;
			auto* Next = Ticket->ReadyNext;
			if (bSolverGone)
			{
				ClosingCursor = Ticket->ActiveNext ? Ticket->ActiveNext : ActiveHead;
				if (!Ticket->bRemovalIssued && !Ticket->bNativeRetired.Load()) continue;
				Ticket->bNativeRetired.Store(true);
			}
			const double Started = FPlatformTime::Seconds();
			if (InFrame && !InFrame->GetPublishBudget().TryStart(EVoxelPublishPhase::Retire,
				Started, RetirementCostMilliseconds)) break;
			const bool bDetached = DetachTicket(*Ticket);
			const double Finished = FPlatformTime::Seconds();
			RetirementCostMilliseconds = FMath::Lerp(RetirementCostMilliseconds, (Finished - Started) * 1000.0, 0.15);
			if (InFrame) InFrame->GetPublishBudget().Finish(EVoxelPublishPhase::Retire, Started, Finished);
			if (!bDetached) break;
			if (!bSolverGone) ReadyBatch = Next;
		}
		if (bClosing && !ActiveHead && !bUnregisterRequested && !bSolverGone)
		{
			bUnregisterRequested = true;
			Solver->UnregisterAndFreeSimCallbackObject_External(Callback);
		}
		return !(bSolverGone && !ActiveHead);
	}
};

struct FVoxelPhysicsRetirement::FCallback final : Chaos::ISimCallbackObject
{
	TSharedPtr<FState, ESPMode::ThreadSafe> State;
	explicit FCallback(TSharedPtr<FState, ESPMode::ThreadSafe> InState)
		: ISimCallbackObject(Chaos::ESimCallbackOptions::Presimulate | Chaos::ESimCallbackOptions::PhysicsObjectUnregister),
		State(MoveTemp(InState)) {}
	virtual ~FCallback() override { State->bCallbackDestroyed.Store(true); }
	virtual void FreeOutputData_External(Chaos::FSimCallbackOutput*) override { checkNoEntry(); }
	virtual void FreeInputData_Internal(Chaos::FSimCallbackInput*) override { checkNoEntry(); }
	virtual Chaos::FSimCallbackInput* AllocateInputData_External() override { checkNoEntry(); return nullptr; }
	virtual FName GetFNameForStatId() const override { return TEXT("VoxelPhysicsRetirement"); }
	virtual void OnPreSimulate_Internal() override { State->AdvancePhysics(); }
	virtual void OnPhysicsObjectUnregistered_Internal(Chaos::FConstPhysicsObjectHandle InObject) override
	{
		State->ObserveDestruction(InObject);
	}
};

struct FVoxelPhysicsRetirement::FTickerTask
{
	TSharedPtr<FState, ESPMode::ThreadSafe> State;
	bool operator()(float) const
	{
		// 活动世界由 Module 消费同一 FrameContext；独立 Ticker 只推进旧世界关闭。
		return !State->bClosing && !State->bCallbackDestroyed.Load() ? true : State->Tick();
	}
};

TSharedPtr<FVoxelPhysicsRetirement, ESPMode::ThreadSafe> FVoxelPhysicsRetirement::Create(
	FPhysScene_Chaos& InScene,
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
	const FVoxelGenerationConsumerKey& InConsumer)
{
	check(IsInGameThread());
	if (!InResources || !InScene.GetSolver()) return nullptr;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	const uint64 Bytes = VoxelBoundedAllocation::WorkerSharedBytes<FState, true>() +
		VoxelBoundedAllocation::WorkerSharedBytes<FVoxelPhysicsRetirement>() +
		FMemory::QuantizeSize(sizeof(FCallback), alignof(FCallback)) +
		VoxelBoundedAllocation::FunctionBytes<FTickerTask>();
	if (!InResources->Acquire(InConsumer, EVoxelTerrainResourcePool::Input, Bytes, Lease)) return nullptr;
	auto Owner = VoxelBoundedAllocation::MakeWorkerShared<FVoxelPhysicsRetirement>(Lease);
	Owner->State = VoxelBoundedAllocation::MakeWorkerShared<FState, true>(Lease);
	Owner->State->MetadataLease = Lease;
	Owner->State->Resources = MoveTemp(InResources);
	Owner->State->Solver = InScene.GetSolver();
	Owner->State->Callback = InScene.GetSolver()->CreateAndRegisterSimCallbackObject_External<FCallback>(Owner->State);
	FTSTicker::GetCoreTicker().AddTicker(TEXT("VoxelPhysicsRetirement"), 0.0f, FTickerTask{Owner->State});
	return Owner;
}

FVoxelPhysicsRetirement::FVoxelPhysicsRetirement()
{
}

FVoxelPhysicsRetirement::~FVoxelPhysicsRetirement()
{
}

bool FVoxelPhysicsRetirement::PrepareTicket(Chaos::FSingleParticlePhysicsProxy* InActor,
	const FVoxelGenerationConsumerKey& InConsumer,
	TSharedPtr<FVoxelPhysicsRetirementPayload, ESPMode::ThreadSafe> InPayload,
	TSharedPtr<FVoxelPhysicsRetirementTicket, ESPMode::ThreadSafe>& InOutTicket,
	bool& bOutCapacityBlocked)
{
	check(IsInGameThread());
	bOutCapacityBlocked = false;
	if (!InActor || !InPayload || State->bClosing || State->bCallbackDestroyed.Load()) return false;
	const auto* InObject = InActor->GetPhysicsObject();
	if (!InObject) return false;
	if (!InOutTicket)
	{
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Metadata, Path;
		const uint64 NodeBytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelPhysicsRetirementNode>();
		if (!State->Resources->Acquire(InConsumer, EVoxelTerrainResourcePool::Input,
			VoxelBoundedAllocation::WorkerSharedBytes<FVoxelPhysicsRetirementTicket, true>() +
			VoxelBoundedAllocation::TaskBytes<FPhysicsPayloadReclaimTask>(), Metadata) ||
			!State->Resources->Acquire(InConsumer, EVoxelTerrainResourcePool::Input, 17 * NodeBytes, Path, 17))
		{
			bOutCapacityBlocked = true;
			return false;
		}
		InOutTicket = VoxelBoundedAllocation::MakeWorkerShared<FVoxelPhysicsRetirementTicket, true>(Metadata);
		InOutTicket->MetadataLease = MoveTemp(Metadata);
		InOutTicket->Consumer = InConsumer;
		InOutTicket->PhysicsObjectKey = uint64(reinterpret_cast<UPTRINT>(InObject));
		InOutTicket->Actor = InActor;
		InOutTicket->Payload = MoveTemp(InPayload);
		for (auto& Node : InOutTicket->PreparedNodes)
		{
			auto NodeLease = Path->Split(NodeBytes);
			check(NodeLease);
			Node = VoxelBoundedAllocation::MakeWorkerShared<FVoxelPhysicsRetirementNode>(MoveTemp(NodeLease));
		}
	}
	if (InOutTicket->bNativeRetired.Load() || InOutTicket->Consumer != InConsumer ||
		InOutTicket->PhysicsObjectKey != uint64(reinterpret_cast<UPTRINT>(InObject))) return false;
	if (InOutTicket->bRegistered) return true;
	if (!State->RootMutex.TryLock()) return false;
	auto* Link = &State->Root;
	for (int32 Depth = 0; Depth <= 16; ++Depth)
	{
		if (!*Link) *Link = MoveTemp(InOutTicket->PreparedNodes[Depth]);
		if (Depth < 16) Link = &(*Link)->Children[PhysicsBranch(InOutTicket->PhysicsObjectKey, Depth)];
	}
	if ((*Link)->Ticket)
	{
		// 地址复用必须等旧票据真正退休；不能替换该地址仍在队列中的身份。
		State->RootMutex.Unlock();
		return false;
	}
	(*Link)->Ticket = InOutTicket;
	InOutTicket->bRegistered = true;
	InOutTicket->ActiveNext = State->ActiveHead;
	if (State->ActiveHead) State->ActiveHead->ActivePrevious = InOutTicket.Get();
	State->ActiveHead = InOutTicket.Get();
	State->RootMutex.Unlock();
	for (auto& Node : InOutTicket->PreparedNodes) Node.Reset();
	return true;
}

void FVoxelPhysicsRetirement::CancelDetached(
	const TSharedPtr<FVoxelPhysicsRetirementTicket, ESPMode::ThreadSafe>& InTicket)
{
	check(IsInGameThread());
	if (!InTicket || InTicket->bInstalled) return;
	if (!InTicket->bNativeRetired.Exchange(true) && InTicket->bRegistered) State->PushReady(*InTicket);
}

void FVoxelPhysicsRetirement::Close()
{
	check(IsInGameThread());
	State->bClosing = true;
}

void FVoxelPhysicsRetirement::Advance(FVoxelTerrainFrameContext& InFrame)
{
	check(IsInGameThread());
	State->Tick(&InFrame);
}

#include "Voxel/Task/VoxelTerrainDependencyPriority.h"

#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/Generation/VoxelGenerationAllocation.h"

FVoxelTerrainDependencyPriority::FVoxelTerrainDependencyPriority(
	TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
	const FVoxelGenerationConsumerKey& InConsumer)
	: Resources(MoveTemp(InResources)), Consumer(InConsumer)
{
	check(Consumer.IsValid());
}

FVoxelTerrainDependencyPriority::~FVoxelTerrainDependencyPriority()
{
}

bool FVoxelTerrainDependencyPriority::PrepareNodes(const int32 InRequired, FString& OutError)
{
	if (InRequired <= 0 || InRequired > (1 << 20))
	{
		OutError = TEXT("Oversized: dependency priority node capacity");
		return false;
	}
	if (!Storage)
	{
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
		if (!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Input,
			VoxelBoundedAllocation::WorkerSharedBytes<FStorage>(), Lease))
		{
			OutError = TEXT("CapacityBlocked: dependency priority control storage");
			return false;
		}
		Storage = VoxelBoundedAllocation::MakeWorkerShared<FStorage>(Lease);
		Storage->ResourceLease = MoveTemp(Lease);
	}
	if (InRequired <= Storage->Capacity) return true;
	const int32 Capacity = FMath::Max(InRequired, FMath::Max(16, Storage->Capacity * 2));
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	if (!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Input,
		VoxelBoundedAllocation::MapBytes<FGuid, FNode>(Capacity), Lease))
	{
		OutError = TEXT("CapacityBlocked: dependency priority node growth");
		return false;
	}
	Storage->Nodes.Reserve(Capacity);
	check(VoxelGenerationAllocation::AllocatedBytes(Storage->Nodes) <= Lease->Bytes());
	Storage->NodesLease = MoveTemp(Lease);
	Storage->Capacity = Capacity;
	return true;
}

bool FVoxelTerrainDependencyPriority::Register(const FGuid& InNode,
	const FVoxelTerrainServicePriority& InOwnPriority, FString& OutError)
{
	if (!InNode.IsValid())
	{
		OutError = TEXT("Invalid dependency priority node identity");
		return false;
	}
	if (Storage && Storage->Nodes.Contains(InNode))
	{
		SetPriority(InNode, InOwnPriority);
		OutError.Reset();
		return true;
	}
	if (!PrepareNodes(Storage ? Storage->Nodes.Num() + 1 : 1, OutError)) return false;
	FNode& Node = Storage->Nodes.Add(InNode);
	Node.Own = Node.Effective = InOwnPriority;
	Dirty(InNode);
	OutError.Reset();
	return true;
}

void FVoxelTerrainDependencyPriority::Dirty(const FGuid& InNode)
{
	FNode* Node = Storage ? Storage->Nodes.Find(InNode) : nullptr;
	if (!Node || Node->bDirty) return;
	Node->bDirty = true;
	Node->PreviousDirty = DirtyTail;
	Node->NextDirty.Invalidate();
	if (DirtyTail.IsValid()) Storage->Nodes.FindChecked(DirtyTail).NextDirty = InNode;
	else DirtyHead = InNode;
	DirtyTail = InNode;
}

void FVoxelTerrainDependencyPriority::UnlinkDirty(FNode& InNodeState)
{
	if (!InNodeState.bDirty) return;
	if (InNodeState.PreviousDirty.IsValid())
		Storage->Nodes.FindChecked(InNodeState.PreviousDirty).NextDirty = InNodeState.NextDirty;
	else DirtyHead = InNodeState.NextDirty;
	if (InNodeState.NextDirty.IsValid())
		Storage->Nodes.FindChecked(InNodeState.NextDirty).PreviousDirty = InNodeState.PreviousDirty;
	else DirtyTail = InNodeState.PreviousDirty;
	InNodeState.PreviousDirty.Invalidate();
	InNodeState.NextDirty.Invalidate();
	InNodeState.bDirty = false;
}

bool FVoxelTerrainDependencyPriority::Reaches(const FGuid& InStart, const FGuid& InTarget,
	bool& bOutReaches, FString& OutError)
{
	bOutReaches = false;
	if (SearchSerial == MAX_uint64)
	{
		OutError = TEXT("Dependency priority search serial exhausted");
		return false;
	}
	const uint64 Serial = ++SearchSerial;
	FGuid Head = InStart;
	FNode& Start = Storage->Nodes.FindChecked(InStart);
	Start.SearchVisited = Serial;
	Start.SearchNext.Invalidate();
	while (Head.IsValid())
	{
		const FGuid Current = Head;
		FNode& Node = Storage->Nodes.FindChecked(Current);
		Head = Node.SearchNext;
		if (Current == InTarget)
		{
			bOutReaches = true;
			return true;
		}
		for (const FGuid& Dependency : Node.Dependencies)
		{
			FNode& Child = Storage->Nodes.FindChecked(Dependency);
			if (Child.SearchVisited == Serial) continue;
			Child.SearchVisited = Serial;
			Child.SearchNext = Head;
			Head = Dependency;
		}
	}
	return true;
}

bool FVoxelTerrainDependencyPriority::SetDependencies(const FGuid& InNode,
	const TSet<FGuid>& InDependencies, FString& OutError)
{
	FNode* Node = Storage ? Storage->Nodes.Find(InNode) : nullptr;
	if (!Node)
	{
		OutError = TEXT("依赖消费者尚未注册");
		return false;
	}
	if (Node->Dependencies.Num() == InDependencies.Num())
	{
		bool bSame = true;
		for (const FGuid& Dependency : InDependencies) bSame &= Node->Dependencies.Contains(Dependency);
		if (bSame)
		{
			OutError.Reset();
			return true;
		}
	}
	int32 GrowthCount = 0;
	for (const FGuid& Dependency : InDependencies)
	{
		FNode* Child = Storage->Nodes.Find(Dependency);
		if (!Child || Dependency == InNode)
		{
			OutError = TEXT("依赖不存在或形成循环");
			return false;
		}
		if (!Node->Dependencies.Contains(Dependency))
		{
			bool bCycle = false;
			if (!Reaches(Dependency, InNode, bCycle, OutError)) return false;
			if (bCycle)
			{
				OutError = TEXT("依赖不存在或形成循环");
				return false;
			}
			if (Child->Consumers.Num() + 1 > Child->ConsumersCapacity) ++GrowthCount;
		}
	}
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> GrowthLease;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> DependenciesLease;
	if ((GrowthCount && !Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Working,
		VoxelBoundedAllocation::ArrayBytes<FConsumerGrowth>(GrowthCount), GrowthLease)) ||
		(InDependencies.Num() && !Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Input,
			VoxelBoundedAllocation::SetBytes<FGuid>(InDependencies.Num()), DependenciesLease)))
	{
		OutError = TEXT("CapacityBlocked: dependency priority edge replacement");
		return false;
	}
	TArray<FConsumerGrowth> Growth;
	Growth.Reserve(GrowthCount);
	for (const FGuid& Dependency : InDependencies)
	{
		const FNode& Child = Storage->Nodes.FindChecked(Dependency);
		if (Node->Dependencies.Contains(Dependency) || Child.Consumers.Num() + 1 <= Child.ConsumersCapacity) continue;
		FConsumerGrowth Plan;
		Plan.Node = Dependency;
		Plan.Capacity = FMath::Max(Child.Consumers.Num() + 1, FMath::Max(4, Child.ConsumersCapacity * 2));
		if (!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Input,
			VoxelBoundedAllocation::SetBytes<FGuid>(Plan.Capacity), Plan.Lease))
		{
			OutError = TEXT("CapacityBlocked: dependency priority reverse consumers");
			return false;
		}
		Growth.Add(MoveTemp(Plan));
	}
	TSet<FGuid> Candidate;
	Candidate.Reserve(InDependencies.Num());
	for (const FGuid& Dependency : InDependencies) Candidate.Add(Dependency);
	check(!DependenciesLease || VoxelGenerationAllocation::AllocatedBytes(Candidate) <= DependenciesLease->Bytes());
	// 所有峰值已授予后，才修改边、反向集合或捐赠状态。
	for (FConsumerGrowth& Plan : Growth)
	{
		FNode& Child = Storage->Nodes.FindChecked(Plan.Node);
		Child.Consumers.Reserve(Plan.Capacity);
		check(VoxelGenerationAllocation::AllocatedBytes(Child.Consumers) <= Plan.Lease->Bytes());
		Child.ConsumersLease = MoveTemp(Plan.Lease);
		Child.ConsumersCapacity = Plan.Capacity;
	}
	for (const FGuid& Dependency : Node->Dependencies)
	{
		if (!Candidate.Contains(Dependency))
		{
			Storage->Nodes.FindChecked(Dependency).Consumers.Remove(InNode);
			Dirty(Dependency);
		}
	}
	for (const FGuid& Dependency : Candidate)
	{
		if (!Node->Dependencies.Contains(Dependency))
		{
			Storage->Nodes.FindChecked(Dependency).Consumers.Add(InNode);
			Dirty(Dependency);
		}
	}
	Node->Dependencies = MoveTemp(Candidate);
	Node->DependenciesLease = MoveTemp(DependenciesLease);
	OutError.Reset();
	return true;
}

void FVoxelTerrainDependencyPriority::SetPriority(const FGuid& InNode,
	const FVoxelTerrainServicePriority& InPriority)
{
	FNode* Node = Storage ? Storage->Nodes.Find(InNode) : nullptr;
	check(Node);
	if (Node->Own != InPriority)
	{
		Node->Own = InPriority;
		Dirty(InNode);
	}
}

void FVoxelTerrainDependencyPriority::Remove(const FGuid& InNode)
{
	FNode* Node = Storage ? Storage->Nodes.Find(InNode) : nullptr;
	if (!Node) return;
	UnlinkDirty(*Node);
	for (const FGuid& Dependency : Node->Dependencies)
	{
		FNode& Child = Storage->Nodes.FindChecked(Dependency);
		Child.Consumers.Remove(InNode);
		Dirty(Dependency);
	}
	for (const FGuid& Parent : Node->Consumers)
		Storage->Nodes.FindChecked(Parent).Dependencies.Remove(InNode);
	Storage->Nodes.Remove(InNode);
}

int32 FVoxelTerrainDependencyPriority::Advance(const int32 InMaximumNodes,
	TFunctionRef<void(const FGuid&, const FVoxelTerrainServicePriority&)> InOnChanged)
{
	int32 Processed = 0;
	while (DirtyHead.IsValid() && Processed < FMath::Max(0, InMaximumNodes))
	{
		const FGuid Key = DirtyHead;
		FNode& Node = Storage->Nodes.FindChecked(Key);
		UnlinkDirty(Node);
		++Processed;
		FVoxelTerrainServicePriority Effective = Node.Own;
		for (const FGuid& Parent : Node.Consumers)
			Effective = FVoxelTerrainWorkSelection::Promote(Effective, Storage->Nodes.FindChecked(Parent).Effective);
		if (Node.Effective == Effective) continue;
		Node.Effective = Effective;
		for (const FGuid& Dependency : Node.Dependencies) Dirty(Dependency);
		// 回调可删除或扩容图，集合遍历在回调前结束。
		InOnChanged(Key, Effective);
	}
	return Processed;
}

const FVoxelTerrainServicePriority* FVoxelTerrainDependencyPriority::Find(const FGuid& InNode) const
{
	const FNode* Node = Storage ? Storage->Nodes.Find(InNode) : nullptr;
	return Node ? &Node->Effective : nullptr;
}

bool FVoxelTerrainDependencyPriority::IsPending() const
{
	return DirtyHead.IsValid();
}

bool FVoxelTerrainDependencyPriority::HasConsumers(const FGuid& InNode) const
{
	const FNode* Node = Storage ? Storage->Nodes.Find(InNode) : nullptr;
	return Node && !Node->Consumers.IsEmpty();
}

int32 FVoxelTerrainDependencyPriority::GetDependencyCount(const FGuid& InNode) const
{
	const FNode* Node = Storage ? Storage->Nodes.Find(InNode) : nullptr;
	return Node ? Node->Dependencies.Num() : INDEX_NONE;
}

bool FVoxelTerrainDependencyPriority::HasDependency(const FGuid& InNode, const FGuid& InDependency) const
{
	const FNode* Node = Storage ? Storage->Nodes.Find(InNode) : nullptr;
	return Node && Node->Dependencies.Contains(InDependency);
}

void FVoxelTerrainDependencyPriority::Reset()
{
	Storage.Reset();
	DirtyHead.Invalidate();
	DirtyTail.Invalidate();
	SearchSerial = 0;
	check(Consumer.OwnerIncarnation != MAX_uint64);
	++Consumer.OwnerIncarnation;
}

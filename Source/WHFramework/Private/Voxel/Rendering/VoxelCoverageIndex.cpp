#include "Voxel/Rendering/VoxelCoverageIndex.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/Task/VoxelTerrainResources.h"
#include "Voxel/Generation/VoxelGenerationAllocation.h"

namespace
{
	TAtomic<uint64> NextVoxelCoverageVersion(0);
	// 复制分支共享 StorageId；空根或 Reset 后的代数必须跨所有分支唯一。
	TAtomic<uint64> NextVoxelCoverageStorageGeneration(1);

	uint64 AllocateCoverageStorageGeneration()
	{
		uint64 Previous = NextVoxelCoverageStorageGeneration.Load();
		for (;;)
		{
			if (Previous == MAX_uint64)
			{
				UE_LOG(LogTemp, Fatal, TEXT("Voxel coverage storage generation exhausted"));
			}
			const uint64 Next = Previous + 1;
			if (NextVoxelCoverageStorageGeneration.CompareExchange(Previous, Next))
			{
				return Next;
			}
		}
	}

	bool SameCoverageRecord(const FVoxelCoverageRecord& InA, const FVoxelCoverageRecord& InB)
	{
		return InA.Geometry == InB.Geometry && InA.Stamp == InB.Stamp && InA.Min == InB.Min && InA.Max == InB.Max &&
			InA.bWanted == InB.bWanted && InA.bReady == InB.bReady &&
			InA.bCommitted == InB.bCommitted && InA.bHasGeometry == InB.bHasGeometry;
	}
}

uint64 FVoxelCoverageIndex::MaximumStorageBytes(const int32 InRecordCount)
{
	check(InRecordCount >= 0);
	// 空根顺序插入时，包含每次新路径、被替代路径和后台销毁控制结构的上界。
	return uint64(InRecordCount) * ((RootLevel + 1) * VoxelBoundedAllocation::WorkerSharedBytes<FNode>() +
		VoxelBoundedAllocation::WorkerSharedBytes<FRoot>() + VoxelBoundedAllocation::WorkerSharedBytes<FVoxelCoverageRecord>());
}

void FVoxelCoverageIndex::ReserveForRecords(const int32 InRecordCount)
{
	check(NumRecords() == 0 && InRecordCount >= 0);
	ReservedRecordLimit = InRecordCount;
}

void FVoxelCoverageIndex::BindResources(TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
	const FVoxelGenerationConsumerKey& InConsumer)
{
	check(InConsumer.IsValid());
	Resources = InResources; Consumer = InConsumer;
}

void FVoxelCoverageIndex::BindStorageLease(TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InLease)
{
	check(!Root && InLease);
	StorageLease = MoveTemp(InLease);
}

FVoxelCoverageIndexKey FVoxelCoverageIndex::IndexKey(const FVoxelTerrainSpatialKey& InKey)
{
	const bool bColumn = InKey.IsColumn();
	return {bColumn ? FIntVector(InKey.Coordinate.X, InKey.Coordinate.Y, 0) : InKey.Coordinate, InKey.Level, bColumn};
}

FVoxelCoverageIndexKey FVoxelCoverageIndex::RootKey(const int32 InSlot)
{
	const bool bColumn = InSlot >= 8;
	const int32 Bits = bColumn ? InSlot - 8 : InSlot;
	return {FIntVector((Bits & 1) ? 0 : -1, (Bits & 2) ? 0 : -1, bColumn ? 0 : ((Bits & 4) ? 0 : -1)), RootLevel, bColumn};
}

FVoxelCoverageIndexKey FVoxelCoverageIndex::ChildKey(const FVoxelCoverageIndexKey& InKey, const int32 InChild)
{
	return {InKey.Coordinate * 2 + FIntVector(InChild & 1, (InChild >> 1) & 1, InKey.bColumn ? 0 : (InChild >> 2) & 1),
		uint8(InKey.Level - 1), InKey.bColumn};
}

int32 FVoxelCoverageIndex::RootSlot(const FVoxelCoverageIndexKey& InKey)
{
	return (InKey.bColumn ? 8 : 0) + (InKey.Coordinate.X >= 0 ? 1 : 0) +
		(InKey.Coordinate.Y >= 0 ? 2 : 0) + (!InKey.bColumn && InKey.Coordinate.Z >= 0 ? 4 : 0);
}

int32 FVoxelCoverageIndex::ChildSlot(const FVoxelCoverageIndexKey& InTarget, const uint8 InParentLevel)
{
	const uint32 Shift = InParentLevel - InTarget.Level - 1;
	return ((uint32(InTarget.Coordinate.X) >> Shift) & 1) |
		(((uint32(InTarget.Coordinate.Y) >> Shift) & 1) << 1) |
		(InTarget.bColumn ? 0 : (((uint32(InTarget.Coordinate.Z) >> Shift) & 1) << 2));
}

const FVoxelCoverageIndex::FNode* FVoxelCoverageIndex::FindNode(const FVoxelCoverageIndexKey& InKey,
	const int32 InVersionKind, uint64& OutGapVersion) const
{
	OutGapVersion = 0;
	if (!Root || InKey.Level > RootLevel) return nullptr;
	const int32 Slot = RootSlot(InKey);
	OutGapVersion = Root->Versions[Slot][InVersionKind];
	const FNode* Node = Root->Nodes[Slot].Get();
	for (uint8 Level = RootLevel; Node && Level > InKey.Level; --Level)
	{
		const int32 Child = ChildSlot(InKey, Level);
		OutGapVersion = Node->ChildVersions[Child][InVersionKind];
		Node = Node->Children[Child].Get();
	}
	return Node;
}

TSharedPtr<const FVoxelCoverageRecord, ESPMode::ThreadSafe> FVoxelCoverageIndex::Find(const FVoxelTerrainSpatialKey& InKey) const
{
	uint64 Gap;
	const auto* Node = FindNode(IndexKey(InKey), 0, Gap);
	const int32 Slot = int32(InKey.Representation) - 1;
	return Node && Slot >= 0 && Slot < 4 ? Node->Records[Slot] : nullptr;
}

int32 FVoxelCoverageIndex::NumRecords() const { return Root ? Root->Records : 0; }
int32 FVoxelCoverageIndex::NumIndexNodes() const { return Root ? Root->NodesCount : 0; }

bool FVoxelCoverageIndex::HasMatchingRecord(const FVoxelCoverageRecord& InRecord) const
{
	const auto Existing = Find(InRecord.Geometry.Key);
	return Existing && SameCoverageRecord(*Existing, InRecord);
}

bool FVoxelCoverageIndex::Upsert(TSharedPtr<const FVoxelCoverageRecord, ESPMode::ThreadSafe> InRecord, FString& OutError)
{
	if (!InRecord) { LastResourceAdmission = EVoxelTerrainResourceAdmission::InvalidRequest; OutError = TEXT("Coverage record is null"); return false; }
	const auto* Value = InRecord.Get();
	return Mutate(Value->Geometry.Key, Value, MoveTemp(InRecord), OutError);
}

bool FVoxelCoverageIndex::Upsert(const FVoxelCoverageRecord& InRecord, FString& OutError)
{
	return Mutate(InRecord.Geometry.Key, &InRecord, nullptr, OutError);
}

bool FVoxelCoverageIndex::Remove(const FVoxelTerrainSpatialKey& InKey)
{
	FString Error;
	return Remove(InKey, Error);
}

bool FVoxelCoverageIndex::Remove(const FVoxelTerrainSpatialKey& InKey, FString& OutError)
{
	if (!Find(InKey)) { LastResourceAdmission = EVoxelTerrainResourceAdmission::InvalidRequest; OutError.Reset(); return false; }
	if (RemoveUnique(InKey))
	{ LastResourceAdmission = EVoxelTerrainResourceAdmission::Granted; OutError.Reset(); return true; }
	return Mutate(InKey, nullptr, nullptr, OutError);
}

bool FVoxelCoverageIndex::Mutate(const FVoxelTerrainSpatialKey& InKey, const FVoxelCoverageRecord* InRecord,
	TSharedPtr<const FVoxelCoverageRecord, ESPMode::ThreadSafe> InSharedRecord, FString& OutError)
{
	LastResourceAdmission = EVoxelTerrainResourceAdmission::InvalidRequest;
	const auto Existing = Find(InKey);
	if (InRecord)
	{
		FIntVector Min, Max;
		if (!InRecord->Geometry.Validate(OutError) ||
			!InKey.TryBounds(InRecord->Geometry.WorldMinZ, InRecord->Geometry.WorldMaxZ, Min, Max)) return false;
		if (InRecord->Min != Min || InRecord->Max != Max)
		{ OutError = TEXT("Coverage records require exact dyadic bounds"); return false; }
		if (Existing && SameCoverageRecord(*Existing, *InRecord))
		{ LastResourceAdmission = EVoxelTerrainResourceAdmission::Granted; OutError.Reset(); return true; }
		if (!Existing && ReservedRecordLimit > 0 && NumRecords() >= ReservedRecordLimit)
		{ LastResourceAdmission = EVoxelTerrainResourceAdmission::CapacityUnachievable; OutError = TEXT("CapacityUnachievable: CoverageIndex record capacity"); return false; }
	}
	const auto Target = IndexKey(InKey);
	const int32 RecordSlot = int32(InKey.Representation) - 1;
	if (RecordSlot < 0 || RecordSlot >= 4 || Target.Level > VoxelStreamingGeometry::MaximumSpatialLevel)
	{ OutError = TEXT("Coverage spatial key is invalid"); return false; }
	const bool bInputsChanged = !Existing || !InRecord || Existing->Geometry != InRecord->Geometry ||
		Existing->Stamp != InRecord->Stamp || Existing->bWanted != InRecord->bWanted ||
		Existing->bReady != InRecord->bReady || Existing->bHasGeometry != InRecord->bHasGeometry;
	const int32 PathCount = RootLevel - Target.Level + 1;
	const uint64 RecordBytes = InRecord ? VoxelBoundedAllocation::WorkerSharedBytes<FVoxelCoverageRecord>() : 0;
	const uint64 Bytes = VoxelBoundedAllocation::WorkerSharedBytes<FRoot>() +
		uint64(PathCount) * VoxelBoundedAllocation::WorkerSharedBytes<FNode>() + RecordBytes;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Grant = StorageLease;
	if (Resources)
	{
		// Serial、表示和空间边界是索引版本身份；容量租约始终属于调用方原 Consumer。
		uint64 OldInput = 0;
		int32 OldLeaseCount = 0;
		if (!AccumulateRetainedInput(*Resources, nullptr, {}, OldInput, OldLeaseCount) ||
			AdditionalRequiredInputBytes > MAX_uint64 - OldInput || AdditionalRequiredLeaseCount < 0 ||
			AdditionalRequiredLeaseCount > MAX_int32 - OldLeaseCount)
		{
			LastResourceAdmission = EVoxelTerrainResourceAdmission::CapacityUnachievable;
			OutError = TEXT("CapacityUnachievable: CoverageIndex retained root input");
			return false;
		}
		OldInput += AdditionalRequiredInputBytes; OldLeaseCount += AdditionalRequiredLeaseCount;
		LastResourceAdmission = Resources->TryAcquire(Consumer, EVoxelTerrainResourcePool::Input, Bytes, Grant,
			PathCount + (InRecord ? 1 : 0), OldInput, OldLeaseCount);
		if (LastResourceAdmission != EVoxelTerrainResourceAdmission::Granted)
		{
			OutError = LastResourceAdmission == EVoxelTerrainResourceAdmission::CapacityBlocked ? TEXT("CapacityBlocked: CoverageIndex changed path") :
				TEXT("CapacityUnachievable: CoverageIndex changed path");
			return false;
		}
	}
	const auto TakeLease = [&Grant, this](const uint64 Count)
	{
		return Resources ? Grant->Split(Count) : Grant;
	};
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> RecordLease;
	if (InRecord && (Grant || !InSharedRecord))
	{
		RecordLease = TakeLease(RecordBytes);
		InSharedRecord = RecordLease ? VoxelBoundedAllocation::MakeWorkerShared<FVoxelCoverageRecord>(RecordLease, *InRecord) :
			MakeShared<FVoxelCoverageRecord, ESPMode::ThreadSafe>(*InRecord);
	}
	auto Candidate = Grant ? VoxelBoundedAllocation::MakeWorkerShared<FRoot>(Grant) : MakeShared<FRoot, ESPMode::ThreadSafe>();
	if (Root) *Candidate = *Root;
	Candidate->Lease = Grant;
	Serial = NextVoxelCoverageVersion.IncrementExchange() + 1;
	check(Serial != 0);
	const int32 Slot = RootSlot(Target);
	Candidate->Nodes[Slot] = MutateNode(Candidate->Nodes[Slot], RootKey(Slot), Target,
		RecordSlot, MoveTemp(InSharedRecord), MoveTemp(RecordLease), bInputsChanged, Grant);
	if (bInputsChanged) Candidate->Versions[Slot][0] = Serial;
	Candidate->Versions[Slot][1] = Serial;
	Candidate->Records = Candidate->NodesCount = 0;
	for (const auto& Node : Candidate->Nodes)
		if (Node) { Candidate->Records += Node->SubtreeRecords; Candidate->NodesCount += Node->SubtreeNodes; }
	Root = MoveTemp(Candidate);
	if (bInputsChanged) MembershipVersion = Serial;
	LastResourceAdmission = EVoxelTerrainResourceAdmission::Granted;
	OutError.Reset();
	return true;
}

FVoxelCoverageIndex::FNodePtr FVoxelCoverageIndex::MutateNode(FNodePtr InPrevious,
	const FVoxelCoverageIndexKey& InNodeKey, const FVoxelCoverageIndexKey& InTarget, const int32 InRecordSlot,
	TSharedPtr<const FVoxelCoverageRecord, ESPMode::ThreadSafe> InRecord,
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InRecordLease, const bool bInInputsChanged,
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InGrant)
{
	const auto Lease = Resources ? InGrant->Split(VoxelBoundedAllocation::WorkerSharedBytes<FNode>()) : InGrant;
	auto Node = Lease ? VoxelBoundedAllocation::MakeWorkerShared<FNode>(Lease) : MakeShared<FNode, ESPMode::ThreadSafe>();
	if (InPrevious) *Node = *InPrevious;
	Node->Lease = Lease;
	if (InNodeKey.Level == InTarget.Level)
	{
		Node->Records[InRecordSlot] = MoveTemp(InRecord);
		Node->RecordLeases[InRecordSlot] = MoveTemp(InRecordLease);
		if (bInInputsChanged) Node->RecordVersions[InRecordSlot][0] = Node->OwnVersions[0] = Serial;
		Node->RecordVersions[InRecordSlot][1] = Node->OwnVersions[1] = Serial;
	}
	else
	{
		const int32 Child = ChildSlot(InTarget, InNodeKey.Level);
		Node->Children[Child] = MutateNode(Node->Children[Child], ChildKey(InNodeKey, Child), InTarget,
			InRecordSlot, MoveTemp(InRecord), MoveTemp(InRecordLease), bInInputsChanged, InGrant);
		if (bInInputsChanged) Node->ChildVersions[Child][0] = Serial;
		Node->ChildVersions[Child][1] = Serial;
	}
	if (bInInputsChanged) Node->SubtreeVersions[0] = Serial;
	Node->SubtreeVersions[1] = Serial;
	RefreshNode(*Node);
	// 空路径立即退休；祖先槽保留单调 gap 版本，取消后的 ABA 不需要永久空节点。
	return Node->SubtreeRecords ? FNodePtr(Node) : nullptr;
}

void FVoxelCoverageIndex::RefreshNode(FNode& InNode) const
{
	InNode.SubtreeRecords = 0; InNode.SubtreeNodes = 1;
	for (const auto& Record : InNode.Records) InNode.SubtreeRecords += Record.IsValid();
	for (const auto& Child : InNode.Children)
		if (Child) { InNode.SubtreeRecords += Child->SubtreeRecords; InNode.SubtreeNodes += Child->SubtreeNodes; }
	InNode.AggregateLedger = Resources.Get();
	InNode.AggregateInputBytes = 0;
	InNode.AggregateLeaseCount = 0;
	InNode.MinimumSerial = MAX_uint64;
	InNode.MaximumSerial = 0;
	if (Resources)
	{
		const auto AddLease = [&InNode, this](const auto& Value)
		{
			if (!Value || !Value->IsOwnedBy(*Resources)) return;
			InNode.AggregateInputBytes += Value->GetPool() == EVoxelTerrainResourcePool::Input ? Value->Bytes() : 0;
			++InNode.AggregateLeaseCount;
			InNode.MinimumSerial = FMath::Min(InNode.MinimumSerial, Value->Serial());
			InNode.MaximumSerial = FMath::Max(InNode.MaximumSerial, Value->Serial());
		};
		AddLease(InNode.Lease);
		for (const auto& Value : InNode.RecordLeases) AddLease(Value);
		for (const auto& Child : InNode.Children)
		{
			if (!Child) continue;
			if (Child->AggregateLedger != Resources.Get()) { InNode.AggregateLedger = nullptr; continue; }
			InNode.AggregateInputBytes += Child->AggregateInputBytes;
			InNode.AggregateLeaseCount += Child->AggregateLeaseCount;
			InNode.MinimumSerial = FMath::Min(InNode.MinimumSerial, Child->MinimumSerial);
			InNode.MaximumSerial = FMath::Max(InNode.MaximumSerial, Child->MaximumSerial);
		}
	}
}

void FVoxelCoverageIndex::MarkPublishedInputs() const
{
	if (!Root) return;
	if (Root->Lease && Root->Lease->IsPublished()) return;
	if (StorageLease) StorageLease->MarkPublished();
	if (Root->Lease) Root->Lease->MarkPublished();
	const auto Mark = [&](auto&& Self, const FNode* Node) -> void
	{
		if (!Node || (Node->Lease && Node->Lease->IsPublished())) return;
		if (Node->Lease) Node->Lease->MarkPublished();
		for (const auto& Lease : Node->RecordLeases) if (Lease) Lease->MarkPublished();
		for (const auto& Child : Node->Children) Self(Self, Child.Get());
	};
	for (const auto& Node : Root->Nodes) Mark(Mark, Node.Get());
}

bool FVoxelCoverageIndex::CanRemoveWithoutAllocation(const FVoxelTerrainSpatialKey& InKey) const
{
	if (!Root || !Root.IsUnique()) return false;
	const auto Target = IndexKey(InKey);
	const int32 Slot = RootSlot(Target), RecordSlot = int32(InKey.Representation) - 1;
	const FNode* Read = Root->Nodes[Slot].Get();
	if (!Root->Nodes[Slot].IsUnique()) return false;
	for (uint8 Level = RootLevel; Read && Level > Target.Level; --Level)
	{
		const int32 Child = ChildSlot(Target, Level);
		if (!Read->Children[Child].IsUnique()) return false;
		Read = Read->Children[Child].Get();
	}
	if (!Read || RecordSlot < 0 || RecordSlot >= 4 || !Read->Records[RecordSlot]) return false;
	return true;
}

bool FVoxelCoverageIndex::RemoveUnique(const FVoxelTerrainSpatialKey& InKey)
{
	if (!CanRemoveWithoutAllocation(InKey)) return false;
	const auto Target = IndexKey(InKey);
	const int32 Slot = RootSlot(Target), RecordSlot = int32(InKey.Representation) - 1;
	// backing 最初由 mutable MakeWorkerShared 创建；整条路径独占后才允许原地退休。
	Serial = NextVoxelCoverageVersion.IncrementExchange() + 1;
	check(Serial != 0);
	auto Retire = [&](auto&& Self, FNodePtr& Link, const uint8 Level) -> void
	{
		FNode* Node = const_cast<FNode*>(Link.Get());
		if (Level == Target.Level)
		{
			Node->Records[RecordSlot].Reset();
			Node->RecordLeases[RecordSlot].Reset();
			Node->RecordVersions[RecordSlot][0] = Node->RecordVersions[RecordSlot][1] = Serial;
			Node->OwnVersions[0] = Node->OwnVersions[1] = Serial;
		}
		else
		{
			const int32 Child = ChildSlot(Target, Level);
			Self(Self, Node->Children[Child], uint8(Level - 1));
			Node->ChildVersions[Child][0] = Node->ChildVersions[Child][1] = Serial;
		}
		Node->SubtreeVersions[0] = Node->SubtreeVersions[1] = Serial;
		RefreshNode(*Node);
		if (!Node->SubtreeRecords) Link.Reset();
	};
	FRoot* Mutable = const_cast<FRoot*>(Root.Get());
	Retire(Retire, Mutable->Nodes[Slot], RootLevel);
	Mutable->Versions[Slot][0] = Mutable->Versions[Slot][1] = Serial;
	Mutable->Records = Mutable->NodesCount = 0;
	for (const auto& Node : Mutable->Nodes)
		if (Node) { Mutable->Records += Node->SubtreeRecords; Mutable->NodesCount += Node->SubtreeNodes; }
	MembershipVersion = Serial;
	if (!Mutable->Records)
	{
		// 空根不再保存 slot/gap 版本；换代防止旧空守卫在插入、删除后复活。
		StorageGeneration = AllocateCoverageStorageGeneration();
		Root.Reset();
	}
	return true;
}

void FVoxelCoverageIndex::VisitChangedRecords(const FVoxelCoverageIndex& InPrevious,
	TFunctionRef<void(const FVoxelCoverageRecord*, const FVoxelCoverageRecord*)> InVisitor) const
{
	const auto Visit = [&](auto&& Self, const FNode* Current, const FNode* Previous) -> void
	{
		if (Current == Previous) return;
		for (int32 Slot = 0; Slot < 4; ++Slot)
		{
			const auto* Now = Current ? Current->Records[Slot].Get() : nullptr;
			const auto* Before = Previous ? Previous->Records[Slot].Get() : nullptr;
			if (Now == Before || (Now && Before && Now->Geometry == Before->Geometry && Now->Stamp == Before->Stamp &&
				Now->bWanted == Before->bWanted && Now->bReady == Before->bReady && Now->bHasGeometry == Before->bHasGeometry)) continue;
			InVisitor(Now, Before);
		}
		for (int32 Child = 0; Child < 8; ++Child)
			Self(Self, Current ? Current->Children[Child].Get() : nullptr, Previous ? Previous->Children[Child].Get() : nullptr);
	};
	for (int32 Slot = 0; Slot < 12; ++Slot)
		Visit(Visit, Root ? Root->Nodes[Slot].Get() : nullptr, InPrevious.Root ? InPrevious.Root->Nodes[Slot].Get() : nullptr);
}

FVoxelCoverageIndex::FQuery FVoxelCoverageIndex::Query(const FIntVector& InMin, const FIntVector& InMax)
{
	return {VoxelStreamingGeometry::FloorDivide(InMin.X, 16) * 16, VoxelStreamingGeometry::FloorDivide(InMin.Y, 16) * 16,
		VoxelStreamingGeometry::FloorDivide(InMin.Z, 16) * 16,
		(VoxelStreamingGeometry::FloorDivide(int64(InMax.X) - 1, 16) + 1) * 16,
		(VoxelStreamingGeometry::FloorDivide(int64(InMax.Y) - 1, 16) + 1) * 16,
		(VoxelStreamingGeometry::FloorDivide(int64(InMax.Z) - 1, 16) + 1) * 16, InMin, InMax};
}

void FVoxelCoverageIndex::CaptureNode(FNodePtr InNode, const FVoxelCoverageIndexKey InKey, const FQuery& InQuery,
	FVoxelCoverageLocalSnapshot& OutSnapshot, const uint64 InGapVersion, const bool bInCollect)
{
	const int64 Side = int64(16) << InKey.Level;
	const int64 X = int64(InKey.Coordinate.X) * Side, Y = int64(InKey.Coordinate.Y) * Side, Z = int64(InKey.Coordinate.Z) * Side;
	if (!bInCollect && (X >= InQuery.MaxX || X + Side <= InQuery.MinX || Y >= InQuery.MaxY || Y + Side <= InQuery.MinY ||
		(!InKey.bColumn && (Z >= InQuery.MaxZ || Z + Side <= InQuery.MinZ)))) return;
	++OutSnapshot.VisitedIndexNodes;
	const bool bCovered = bInCollect || !InNode || InKey.Level == 0 ||
		(InQuery.MinX <= X && InQuery.MaxX >= X + Side && InQuery.MinY <= Y && InQuery.MaxY >= Y + Side &&
			(InKey.bColumn || (InQuery.MinZ <= Z && InQuery.MaxZ >= Z + Side)));
	const int32 V = OutSnapshot.bIncludePresentation ? 1 : 0;
	if (!bInCollect)
	{
		++OutSnapshot.GuardCount;
		if (!OutSnapshot.bCountOnly) OutSnapshot.Guards.Add({InKey,
			InNode ? (bCovered ? InNode->SubtreeVersions[V] : InNode->OwnVersions[V]) : InGapVersion, bCovered});
	}
	if (!InNode) return;
	for (int32 Slot = 0; Slot < 4; ++Slot)
	{
		const auto& Record = InNode->Records[Slot];
		if (!Record) continue;
		++OutSnapshot.ExaminedRecords;
		bool bIntersects = true;
		for (int32 Axis = 0; Axis < 3; ++Axis)
			bIntersects &= Record->Min[Axis] < InQuery.ExactMax[Axis] && Record->Max[Axis] > InQuery.ExactMin[Axis];
		if (bIntersects)
		{
			++OutSnapshot.MatchedRecords;
			if (!OutSnapshot.bCountOnly)
			{
				OutSnapshot.Records.Add(Record);
				OutSnapshot.RecordGuards.Add({Record->Geometry.Key, InNode->RecordVersions[Slot][V]});
			}
		}
	}
	if (InKey.Level == 0) return;
	for (int32 Child = 0; Child < (InKey.bColumn ? 4 : 8); ++Child)
		CaptureNode(InNode->Children[Child], ChildKey(InKey, Child), InQuery, OutSnapshot, InNode->ChildVersions[Child][V], bCovered);
}

bool FVoxelCoverageIndex::AccumulateRetainedInput(const FVoxelTerrainResources& InResources, const FVoxelCoverageIndex* InOther,
	TConstArrayView<uint64> InExcludedSerials, uint64& InOutBytes, int32& InOutLeaseCount) const
{
	const FVoxelCoverageIndex* Indexes[] = {this, InOther};
	return AccumulateRetainedInputs(InResources, MakeArrayView(Indexes), InExcludedSerials, InOutBytes, InOutLeaseCount);
}

bool FVoxelCoverageIndex::AccumulateRetainedInputs(const FVoxelTerrainResources& InResources,
	TConstArrayView<const FVoxelCoverageIndex*> InIndexes, TConstArrayView<uint64> InExcludedSerials,
	uint64& InOutBytes, int32& InOutLeaseCount)
{
	// 最多三十二份冻结根；每层最多三十二条 COW 路径，记录只在同空间槽出现。
	if (InIndexes.Num() > 32 || InOutLeaseCount < 0) return false;
	uint64 Ancestors[32 * (RootLevel + 4)] = {};
	int32 AncestorCount = 0;
	const auto HasSerial = [&](const uint64 Serial)
	{
		for (const uint64 Excluded : InExcludedSerials) if (Serial == Excluded) return true;
		for (int32 Index = 0; Index < AncestorCount; ++Index) if (Serial == Ancestors[Index]) return true;
		return false;
	};
	const auto AddLease = [&](const TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& Value)
	{
		if (!Value || !Value->IsOwnedBy(InResources) || HasSerial(Value->Serial())) return true;
		const uint64 Bytes = Value->GetPool() == EVoxelTerrainResourcePool::Input ? Value->Bytes() : 0;
		if (Bytes > MAX_uint64 - InOutBytes || InOutLeaseCount == MAX_int32 || AncestorCount == UE_ARRAY_COUNT(Ancestors)) return false;
		InOutBytes += Bytes; ++InOutLeaseCount;
		Ancestors[AncestorCount++] = Value->Serial();
		return true;
	};
	for (const auto* Index : InIndexes)
		if (Index && (!AddLease(Index->StorageLease) || !AddLease(Index->Root ? Index->Root->Lease : nullptr))) return false;
	const auto Visit = [&](auto&& Self, const FNode* const* Nodes) -> bool
	{
		const FNode* Shared = nullptr;
		bool bDifferent = false;
		for (int32 Index = 0; Index < InIndexes.Num(); ++Index)
		{
			if (!Nodes[Index]) continue;
			if (!Shared) Shared = Nodes[Index];
			else bDifferent |= Shared != Nodes[Index];
		}
		if (!Shared) return true;
		if (!bDifferent && Shared->AggregateLedger == &InResources)
		{
			bool bExcludedRange = false;
			for (const uint64 Serial : InExcludedSerials)
				bExcludedRange |= Serial >= Shared->MinimumSerial && Serial <= Shared->MaximumSerial;
			for (int32 Index = 0; Index < AncestorCount; ++Index)
				bExcludedRange |= Ancestors[Index] >= Shared->MinimumSerial && Ancestors[Index] <= Shared->MaximumSerial;
			if (!bExcludedRange)
			{
				if (Shared->AggregateInputBytes > MAX_uint64 - InOutBytes || Shared->AggregateLeaseCount > MAX_int32 - InOutLeaseCount) return false;
				InOutBytes += Shared->AggregateInputBytes; InOutLeaseCount += Shared->AggregateLeaseCount;
				return true;
			}
		}
		const int32 Before = AncestorCount;
		for (int32 Index = 0; Index < InIndexes.Num(); ++Index)
			if (Nodes[Index] && !AddLease(Nodes[Index]->Lease)) return false;
		const int32 PathCount = AncestorCount;
		for (int32 Slot = 0; Slot < 4; ++Slot)
		{
			for (int32 Index = 0; Index < InIndexes.Num(); ++Index)
				if (Nodes[Index] && !AddLease(Nodes[Index]->RecordLeases[Slot])) return false;
			AncestorCount = PathCount;
		}
		for (int32 Child = 0; Child < 8; ++Child)
		{
			const FNode* Children[32] = {};
			for (int32 Index = 0; Index < InIndexes.Num(); ++Index)
				Children[Index] = Nodes[Index] ? Nodes[Index]->Children[Child].Get() : nullptr;
			if (!Self(Self, Children)) return false;
		}
		AncestorCount = Before;
		return true;
	};
	for (int32 Slot = 0; Slot < 12; ++Slot)
	{
		const FNode* Nodes[32] = {};
		for (int32 Index = 0; Index < InIndexes.Num(); ++Index)
			Nodes[Index] = InIndexes[Index] && InIndexes[Index]->Root ? InIndexes[Index]->Root->Nodes[Slot].Get() : nullptr;
		if (!Visit(Visit, Nodes)) return false;
	}
	return true;
}

bool FVoxelCoverageIndex::ReserveSnapshot(FVoxelCoverageLocalSnapshot& InOutSnapshot) const
{
	const uint64 Bytes =
		FMath::Max(VoxelGenerationAllocation::AllocatedBytes(InOutSnapshot.Records.Values()),
			VoxelGenerationAllocation::Private::ArrayReserveBytes<TSharedPtr<const FVoxelCoverageRecord, ESPMode::ThreadSafe>, FDefaultAllocator>(InOutSnapshot.MatchedRecords)) +
		FMath::Max(VoxelGenerationAllocation::AllocatedBytes(InOutSnapshot.RecordGuards.Values()),
			VoxelGenerationAllocation::Private::ArrayReserveBytes<FVoxelCoverageRecordReadGuard, FDefaultAllocator>(InOutSnapshot.MatchedRecords)) +
		FMath::Max(VoxelGenerationAllocation::AllocatedBytes(InOutSnapshot.Guards.Values()),
			VoxelGenerationAllocation::Private::ArrayReserveBytes<FVoxelCoverageReadGuard, FDefaultAllocator>(InOutSnapshot.GuardCount)) +
		FVoxelCoverageLocalSnapshot::ArrayMetadataBytes();
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> CandidateLease;
	InOutSnapshot.ResourceAdmission = EVoxelTerrainResourceAdmission::Granted;
	if (Resources && Bytes)
	{
		uint64 RequiredInput = 0;
		int32 RequiredLeaseCount = 0;
		const FVoxelTerrainResourceLease* Seen[3] = {};
		for (const auto& Lease : {InOutSnapshot.ResourceLease, StorageLease, Root ? Root->Lease : nullptr})
		{
			if (!Lease || !Lease->IsOwnedBy(*Resources)) continue;
			bool bSeen = false;
			for (int32 Index = 0; Index < RequiredLeaseCount; ++Index) bSeen |= Seen[Index] == Lease.Get();
			if (bSeen) continue;
			const uint64 Input = Lease->GetPool() == EVoxelTerrainResourcePool::Input ? Lease->Bytes() : 0;
			if (Input > MAX_uint64 - RequiredInput)
			{
				InOutSnapshot.ResourceAdmission = EVoxelTerrainResourceAdmission::CapacityUnachievable;
				return false;
			}
			RequiredInput += Input;
			Seen[RequiredLeaseCount++] = Lease.Get();
		}
		uint64 Excluded[3] = {};
		for (int32 Index = 0; Index < RequiredLeaseCount; ++Index) Excluded[Index] = Seen[Index]->Serial();
		if (!AccumulateRetainedInput(*Resources, nullptr, MakeArrayView(Excluded, RequiredLeaseCount), RequiredInput, RequiredLeaseCount) ||
			AdditionalRequiredInputBytes > MAX_uint64 - RequiredInput || AdditionalRequiredLeaseCount < 0 ||
			AdditionalRequiredLeaseCount > MAX_int32 - RequiredLeaseCount)
		{
			InOutSnapshot.ResourceAdmission = EVoxelTerrainResourceAdmission::CapacityUnachievable;
			return false;
		}
		RequiredInput += AdditionalRequiredInputBytes; RequiredLeaseCount += AdditionalRequiredLeaseCount;
		InOutSnapshot.ResourceAdmission = Resources->TryAcquire(Consumer, EVoxelTerrainResourcePool::Input,
			Bytes, CandidateLease, 0, RequiredInput, RequiredLeaseCount);
		if (InOutSnapshot.ResourceAdmission != EVoxelTerrainResourceAdmission::Granted) return false;
	}
	else CandidateLease = StorageLease ? StorageLease : InOutSnapshot.ResourceLease;
	InOutSnapshot.Records.BindLease(CandidateLease);
	InOutSnapshot.RecordGuards.BindLease(CandidateLease);
	InOutSnapshot.Guards.BindLease(CandidateLease);
	InOutSnapshot.Records.Reserve(InOutSnapshot.MatchedRecords);
	InOutSnapshot.RecordGuards.Reserve(InOutSnapshot.MatchedRecords);
	InOutSnapshot.Guards.Reserve(InOutSnapshot.GuardCount);
	InOutSnapshot.ResourceLease = MoveTemp(CandidateLease);
	return true;
}

void FVoxelCoverageIndex::Capture(const FIntVector& InMin, const FIntVector& InMax,
	FVoxelCoverageLocalSnapshot& OutSnapshot, const bool bInIncludePresentation, const bool bInCountOnly) const
{
	OutSnapshot.Records.Reset(); OutSnapshot.RecordGuards.Reset(); OutSnapshot.Guards.Reset();
	OutSnapshot.StorageId = StorageId;
	OutSnapshot.StorageGeneration = StorageGeneration; OutSnapshot.MembershipVersion = MembershipVersion;
	OutSnapshot.bFullIndex = false; OutSnapshot.bIncludePresentation = bInIncludePresentation;
	OutSnapshot.bChangeSet = false;
	OutSnapshot.bCountOnly = true;
	OutSnapshot.ResourceAdmission = EVoxelTerrainResourceAdmission::None;
	OutSnapshot.VisitedIndexNodes = OutSnapshot.ExaminedRecords = OutSnapshot.MatchedRecords = OutSnapshot.GuardCount = 0;
	if (InMin.X >= InMax.X || InMin.Y >= InMax.Y || InMin.Z >= InMax.Z) return;
	const auto Bounds = Query(InMin, InMax);
	const int32 V = bInIncludePresentation ? 1 : 0;
	for (int32 Slot = 0; Slot < 12; ++Slot)
		CaptureNode(Root ? Root->Nodes[Slot] : nullptr, RootKey(Slot), Bounds, OutSnapshot, Root ? Root->Versions[Slot][V] : 0);
	if (bInCountOnly) return;
	if (!ReserveSnapshot(OutSnapshot)) return; // count-only 不能作为有效读守卫。
	OutSnapshot.bCountOnly = false;
	OutSnapshot.VisitedIndexNodes = OutSnapshot.ExaminedRecords = OutSnapshot.MatchedRecords = OutSnapshot.GuardCount = 0;
	for (int32 Slot = 0; Slot < 12; ++Slot)
		CaptureNode(Root ? Root->Nodes[Slot] : nullptr, RootKey(Slot), Bounds, OutSnapshot, Root ? Root->Versions[Slot][V] : 0);
}

bool FVoxelCoverageIndex::IsCurrent(const FVoxelCoverageLocalSnapshot& InSnapshot) const
{
	if (InSnapshot.bCountOnly || InSnapshot.bChangeSet || InSnapshot.StorageId != StorageId || InSnapshot.StorageGeneration != StorageGeneration ||
		(InSnapshot.bFullIndex && InSnapshot.MembershipVersion != MembershipVersion)) return false;
	const int32 V = InSnapshot.bIncludePresentation ? 1 : 0;
	for (const auto& Guard : InSnapshot.Guards)
	{
		uint64 Gap;
		const auto* Node = FindNode(Guard.Key, V, Gap);
		const uint64 Version = Node ? (Guard.bSubtree ? Node->SubtreeVersions[V] : Node->OwnVersions[V]) : Gap;
		if (Version != Guard.Version) return false;
	}
	for (const auto& Guard : InSnapshot.RecordGuards)
	{
		uint64 Gap;
		const auto* Node = FindNode(IndexKey(Guard.Key), V, Gap);
		const int32 Slot = int32(Guard.Key.Representation) - 1;
		if (!Node || !Node->Records[Slot] || Node->RecordVersions[Slot][V] != Guard.Version) return false;
	}
	return true;
}

bool FVoxelCoverageIndex::CompareNodeRange(FNodePtr InCurrent, FNodePtr InFrozen, const FVoxelCoverageIndexKey InKey,
	const FQuery& InQuery, const int32 V, const uint64 InCurrentGap, const uint64 InFrozenGap)
{
	const int64 Side = int64(16) << InKey.Level;
	const int64 X = int64(InKey.Coordinate.X) * Side, Y = int64(InKey.Coordinate.Y) * Side, Z = int64(InKey.Coordinate.Z) * Side;
	if (X >= InQuery.MaxX || X + Side <= InQuery.MinX || Y >= InQuery.MaxY || Y + Side <= InQuery.MinY ||
		(!InKey.bColumn && (Z >= InQuery.MaxZ || Z + Side <= InQuery.MinZ))) return true;
	if (InCurrent == InFrozen) return InCurrent || InCurrentGap == InFrozenGap;
	const uint64 CurrentVersion = InCurrent ? InCurrent->SubtreeVersions[V] : InCurrentGap;
	const uint64 FrozenVersion = InFrozen ? InFrozen->SubtreeVersions[V] : InFrozenGap;
	if (CurrentVersion == FrozenVersion) return true;
	if (InQuery.MinX <= X && InQuery.MaxX >= X + Side && InQuery.MinY <= Y && InQuery.MaxY >= Y + Side &&
		(InKey.bColumn || (InQuery.MinZ <= Z && InQuery.MaxZ >= Z + Side))) return false;
	for (int32 Slot = 0; Slot < 4; ++Slot)
	{
		const uint64 Current = InCurrent ? InCurrent->RecordVersions[Slot][V] : 0;
		const uint64 Frozen = InFrozen ? InFrozen->RecordVersions[Slot][V] : 0;
		if (Current == Frozen) continue;
		const auto Intersects = [&InQuery](const FVoxelCoverageRecord* Record)
		{
			return Record && Record->Min.X < InQuery.ExactMax.X && Record->Max.X > InQuery.ExactMin.X &&
				Record->Min.Y < InQuery.ExactMax.Y && Record->Max.Y > InQuery.ExactMin.Y &&
				Record->Min.Z < InQuery.ExactMax.Z && Record->Max.Z > InQuery.ExactMin.Z;
		};
		if (Intersects(InCurrent ? InCurrent->Records[Slot].Get() : nullptr) ||
			Intersects(InFrozen ? InFrozen->Records[Slot].Get() : nullptr)) return false;
	}
	if (InKey.Level == 0) return false; // 最小 Atom 的 gap 代同样拒绝插入/删除 ABA。
	for (int32 Child = 0; Child < (InKey.bColumn ? 4 : 8); ++Child)
		if (!CompareNodeRange(InCurrent ? InCurrent->Children[Child] : nullptr, InFrozen ? InFrozen->Children[Child] : nullptr,
			ChildKey(InKey, Child), InQuery, V, InCurrent ? InCurrent->ChildVersions[Child][V] : InCurrentGap,
			InFrozen ? InFrozen->ChildVersions[Child][V] : InFrozenGap)) return false;
	return true;
}

bool FVoxelCoverageIndex::IsRangeCurrent(const FVoxelCoverageIndex& InFrozen, const FIntVector& InMin,
	const FIntVector& InMax, const bool bInIncludePresentation) const
{
	if (StorageId != InFrozen.StorageId || StorageGeneration != InFrozen.StorageGeneration ||
		InMin.X >= InMax.X || InMin.Y >= InMax.Y || InMin.Z >= InMax.Z) return false;
	const int32 V = bInIncludePresentation ? 1 : 0;
	const auto Bounds = Query(InMin, InMax);
	for (int32 Slot = 0; Slot < 12; ++Slot)
		if (!CompareNodeRange(Root ? Root->Nodes[Slot] : nullptr, InFrozen.Root ? InFrozen.Root->Nodes[Slot] : nullptr,
			RootKey(Slot), Bounds, V, Root ? Root->Versions[Slot][V] : 0, InFrozen.Root ? InFrozen.Root->Versions[Slot][V] : 0)) return false;
	return true;
}

bool FVoxelCoverageIndex::AnyNodeRecord(FNodePtr InNode, const FVoxelCoverageIndexKey InKey, const FQuery& InQuery,
	TFunctionRef<bool(const FVoxelCoverageRecord&)> InPredicate)
{
	if (!InNode) return false;
	const int64 Side = int64(16) << InKey.Level;
	const int64 X = int64(InKey.Coordinate.X) * Side, Y = int64(InKey.Coordinate.Y) * Side, Z = int64(InKey.Coordinate.Z) * Side;
	if (X >= InQuery.MaxX || X + Side <= InQuery.MinX || Y >= InQuery.MaxY || Y + Side <= InQuery.MinY ||
		(!InKey.bColumn && (Z >= InQuery.MaxZ || Z + Side <= InQuery.MinZ))) return false;
	for (const auto& Record : InNode->Records)
	{
		if (!Record) continue;
		bool bIntersects = true;
		for (int32 Axis = 0; Axis < 3; ++Axis)
			bIntersects &= Record->Min[Axis] < InQuery.ExactMax[Axis] && Record->Max[Axis] > InQuery.ExactMin[Axis];
		if (bIntersects && InPredicate(*Record)) return true;
	}
	if (InKey.Level == 0) return false;
	for (int32 Child = 0; Child < (InKey.bColumn ? 4 : 8); ++Child)
		if (AnyNodeRecord(InNode->Children[Child], ChildKey(InKey, Child), InQuery, InPredicate)) return true;
	return false;
}

bool FVoxelCoverageIndex::AnyRecord(const FIntVector& InMin, const FIntVector& InMax,
	TFunctionRef<bool(const FVoxelCoverageRecord&)> InPredicate) const
{
	if (!Root || InMin.X >= InMax.X || InMin.Y >= InMax.Y || InMin.Z >= InMax.Z) return false;
	const auto Frozen = Root;
	const auto Bounds = Query(InMin, InMax);
	for (int32 Slot = 0; Slot < 12; ++Slot)
		if (AnyNodeRecord(Frozen->Nodes[Slot], RootKey(Slot), Bounds, InPredicate)) return true;
	return false;
}

void FVoxelCoverageIndex::PruneEmptyNodes(const int32 InOutstandingSnapshots)
{
	// 空节点已在单路径更新时退休，冻结根与 gap 版本不受全局清理影响。
	(void)InOutstandingSnapshots;
}

void FVoxelCoverageIndex::Reset()
{
	const uint64 NewStorageGeneration = AllocateCoverageStorageGeneration();
	Root.Reset(); StorageLease.Reset(); ReservedRecordLimit = 0;
	// Serial 不回退；独立重置的复制分支也不能共用相同的 StorageGeneration。
	MembershipVersion = Serial;
	StorageGeneration = NewStorageGeneration;
}

FVoxelGameThreadContinuation FVoxelCoverageIndex::CaptureNodeSteps(FNodePtr InNode, const FVoxelCoverageIndexKey InKey,
	const FQuery InQuery, FVoxelCoverageLocalSnapshot& OutSnapshot, const uint64 InGapVersion, const bool bInCollect)
{
	co_await FVoxelGameThreadContinuation::Yield();
	const int64 Side = int64(16) << InKey.Level;
	const int64 X = int64(InKey.Coordinate.X) * Side, Y = int64(InKey.Coordinate.Y) * Side, Z = int64(InKey.Coordinate.Z) * Side;
	if (!bInCollect && (X >= InQuery.MaxX || X + Side <= InQuery.MinX || Y >= InQuery.MaxY || Y + Side <= InQuery.MinY ||
		(!InKey.bColumn && (Z >= InQuery.MaxZ || Z + Side <= InQuery.MinZ)))) co_return;
	++OutSnapshot.VisitedIndexNodes;
	const bool bCovered = bInCollect || !InNode || InKey.Level == 0 ||
		(InQuery.MinX <= X && InQuery.MaxX >= X + Side && InQuery.MinY <= Y && InQuery.MaxY >= Y + Side &&
			(InKey.bColumn || (InQuery.MinZ <= Z && InQuery.MaxZ >= Z + Side)));
	const int32 V = OutSnapshot.bIncludePresentation ? 1 : 0;
	if (!bInCollect)
	{
		++OutSnapshot.GuardCount;
		if (!OutSnapshot.bCountOnly) OutSnapshot.Guards.Add({InKey,
			InNode ? (bCovered ? InNode->SubtreeVersions[V] : InNode->OwnVersions[V]) : InGapVersion, bCovered});
	}
	if (!InNode) co_return;
	for (int32 Slot = 0; Slot < 4; ++Slot)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		const auto& Record = InNode->Records[Slot];
		if (!Record) continue;
		++OutSnapshot.ExaminedRecords;
		bool bIntersects = true;
		for (int32 Axis = 0; Axis < 3; ++Axis)
			bIntersects &= Record->Min[Axis] < InQuery.ExactMax[Axis] && Record->Max[Axis] > InQuery.ExactMin[Axis];
		if (bIntersects)
		{
			++OutSnapshot.MatchedRecords;
			if (!OutSnapshot.bCountOnly)
			{
				OutSnapshot.Records.Add(Record);
				OutSnapshot.RecordGuards.Add({Record->Geometry.Key, InNode->RecordVersions[Slot][V]});
			}
		}
	}
	if (InKey.Level == 0) co_return;
	for (int32 Child = 0; Child < (InKey.bColumn ? 4 : 8); ++Child)
		co_await CaptureNodeSteps(InNode->Children[Child], ChildKey(InKey, Child), InQuery, OutSnapshot,
			InNode->ChildVersions[Child][V], bCovered);
}

FVoxelGameThreadContinuation FVoxelCoverageIndex::CaptureFrozenSteps(FVoxelCoverageIndex InFrozen, const FQuery InQuery,
	FVoxelCoverageLocalSnapshot& OutSnapshot, const bool bInIncludePresentation, const bool bInFullIndex, const bool bInWaitForCapacity)
{
	OutSnapshot = {};
	OutSnapshot.StorageId = InFrozen.StorageId;
	OutSnapshot.StorageGeneration = InFrozen.StorageGeneration; OutSnapshot.MembershipVersion = InFrozen.MembershipVersion;
	OutSnapshot.bFullIndex = bInFullIndex; OutSnapshot.bIncludePresentation = bInIncludePresentation; OutSnapshot.bCountOnly = true;
	OutSnapshot.ResourceAdmission = EVoxelTerrainResourceAdmission::None;
	if (InQuery.ExactMin.X >= InQuery.ExactMax.X || InQuery.ExactMin.Y >= InQuery.ExactMax.Y || InQuery.ExactMin.Z >= InQuery.ExactMax.Z) co_return;
	const int32 V = bInIncludePresentation ? 1 : 0;
	for (int32 Slot = 0; Slot < 12; ++Slot)
		co_await CaptureNodeSteps(InFrozen.Root ? InFrozen.Root->Nodes[Slot] : nullptr, RootKey(Slot), InQuery, OutSnapshot,
			InFrozen.Root ? InFrozen.Root->Versions[Slot][V] : 0);
	for (;;)
	{
		const uint64 BeforeAdmission = InFrozen.Resources ? InFrozen.Resources->ReleaseVersionExcludingOwner(InFrozen.Consumer) : 0;
		if (InFrozen.ReserveSnapshot(OutSnapshot)) break;
		if (!bInWaitForCapacity || !InFrozen.Resources || OutSnapshot.ResourceAdmission != EVoxelTerrainResourceAdmission::CapacityBlocked) co_return;
		uint64 Version = BeforeAdmission;
		do { co_await FVoxelGameThreadContinuation::Yield(); }
		while (!InFrozen.Resources->TryGetReleaseVersionExcludingOwner(InFrozen.Consumer, Version) || Version <= BeforeAdmission);
	}
	OutSnapshot.bCountOnly = false;
	OutSnapshot.VisitedIndexNodes = OutSnapshot.ExaminedRecords = OutSnapshot.MatchedRecords = OutSnapshot.GuardCount = 0;
	for (int32 Slot = 0; Slot < 12; ++Slot)
		co_await CaptureNodeSteps(InFrozen.Root ? InFrozen.Root->Nodes[Slot] : nullptr, RootKey(Slot), InQuery, OutSnapshot,
			InFrozen.Root ? InFrozen.Root->Versions[Slot][V] : 0);
}

FVoxelGameThreadContinuation FVoxelCoverageIndex::CaptureSteps(const FIntVector InMin, const FIntVector InMax,
	FVoxelCoverageLocalSnapshot& OutSnapshot, const bool bInIncludePresentation, const bool bInWaitForCapacity) const
{
	// 此函数本身不是 coroutine；调用时立即复制 O(1) 根，不能延迟至首次 Resume 才读 Current。
	return CaptureFrozenSteps(*this, Query(InMin, InMax), OutSnapshot, bInIncludePresentation, false, bInWaitForCapacity);
}

FVoxelGameThreadContinuation FVoxelCoverageIndex::CaptureAllSteps(FVoxelCoverageLocalSnapshot& OutSnapshot, const bool bInIncludePresentation, const bool bInWaitForCapacity) const
{
	return CaptureFrozenSteps(*this, Query(FIntVector(MIN_int32), FIntVector(MAX_int32)), OutSnapshot, bInIncludePresentation, true, bInWaitForCapacity);
}

FVoxelGameThreadContinuation FVoxelCoverageIndex::CaptureChangedNodeSteps(FNodePtr InCurrent, FNodePtr InPrevious,
	FVoxelCoverageLocalSnapshot& OutSnapshot)
{
	co_await FVoxelGameThreadContinuation::Yield();
	if (!InCurrent || InCurrent == InPrevious) co_return;
	++OutSnapshot.VisitedIndexNodes;
	for (int32 Slot = 0; Slot < 4; ++Slot)
	{
		co_await FVoxelGameThreadContinuation::Yield();
		const auto& Record = InCurrent->Records[Slot];
		if (!Record || (InPrevious && InCurrent->RecordVersions[Slot][0] == InPrevious->RecordVersions[Slot][0])) continue;
		++OutSnapshot.MatchedRecords;
		if (!OutSnapshot.bCountOnly) OutSnapshot.Records.Add(Record);
	}
	for (int32 Child = 0; Child < 8; ++Child)
		co_await CaptureChangedNodeSteps(InCurrent->Children[Child], InPrevious ? InPrevious->Children[Child] : nullptr, OutSnapshot);
}

FVoxelGameThreadContinuation FVoxelCoverageIndex::CaptureChangedFrozenSteps(FVoxelCoverageIndex InFrozen,
	FVoxelCoverageIndex InPrevious, FVoxelCoverageLocalSnapshot& OutSnapshot)
{
	OutSnapshot = {};
	OutSnapshot.bChangeSet = true;
	OutSnapshot.bCountOnly = true;
	OutSnapshot.ResourceAdmission = EVoxelTerrainResourceAdmission::None;
	OutSnapshot.StorageId = InFrozen.StorageId;
	OutSnapshot.StorageGeneration = InFrozen.StorageGeneration;
	OutSnapshot.MembershipVersion = InFrozen.MembershipVersion;
	for (int32 Pass = 0; Pass < 2; ++Pass)
	{
		OutSnapshot.MatchedRecords = OutSnapshot.VisitedIndexNodes = 0;
		for (int32 Slot = 0; Slot < 12; ++Slot)
			co_await CaptureChangedNodeSteps(InFrozen.Root ? InFrozen.Root->Nodes[Slot] : nullptr,
				InPrevious.StorageId == InFrozen.StorageId && InPrevious.StorageGeneration == InFrozen.StorageGeneration && InPrevious.Root ?
					InPrevious.Root->Nodes[Slot] : nullptr, OutSnapshot);
		if (Pass == 0)
		{
			for (;;)
			{
				const uint64 BeforeAdmission = InFrozen.Resources ? InFrozen.Resources->ReleaseVersionExcludingOwner(InFrozen.Consumer) : 0;
				if (InFrozen.ReserveSnapshot(OutSnapshot)) break;
				if (!InFrozen.Resources || OutSnapshot.ResourceAdmission != EVoxelTerrainResourceAdmission::CapacityBlocked) co_return;
				uint64 Version = BeforeAdmission;
				do { co_await FVoxelGameThreadContinuation::Yield(); }
				while (!InFrozen.Resources->TryGetReleaseVersionExcludingOwner(InFrozen.Consumer, Version) || Version <= BeforeAdmission);
			}
			OutSnapshot.bCountOnly = false;
		}
	}
}

FVoxelGameThreadContinuation FVoxelCoverageIndex::CaptureChangedSteps(const FVoxelCoverageIndex& InPrevious,
	FVoxelCoverageLocalSnapshot& OutSnapshot) const
{
	return CaptureChangedFrozenSteps(*this, InPrevious, OutSnapshot);
}

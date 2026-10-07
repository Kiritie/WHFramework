#include "Voxel/Rendering/VoxelCoverageIndex.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/Task/VoxelTerrainResources.h"

namespace
{
	TAtomic<uint64> NextVoxelCoverageVersion(0);
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

bool FVoxelCoverageIndex::Upsert(TSharedPtr<const FVoxelCoverageRecord, ESPMode::ThreadSafe> InRecord, FString& OutError)
{
	if (!InRecord) { OutError = TEXT("Coverage record is null"); return false; }
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
	if (!Find(InKey)) { OutError.Reset(); return false; }
	return Mutate(InKey, nullptr, nullptr, OutError);
}

bool FVoxelCoverageIndex::Mutate(const FVoxelTerrainSpatialKey& InKey, const FVoxelCoverageRecord* InRecord,
	TSharedPtr<const FVoxelCoverageRecord, ESPMode::ThreadSafe> InSharedRecord, FString& OutError)
{
	const auto Existing = Find(InKey);
	if (InRecord)
	{
		FIntVector Min, Max;
		if (!InRecord->Geometry.Validate(OutError) ||
			!InKey.TryBounds(InRecord->Geometry.WorldMinZ, InRecord->Geometry.WorldMaxZ, Min, Max)) return false;
		if (InRecord->Min != Min || InRecord->Max != Max)
		{ OutError = TEXT("Coverage records require exact dyadic bounds"); return false; }
		if (Existing && Existing->Geometry == InRecord->Geometry && Existing->Stamp == InRecord->Stamp &&
			Existing->bWanted == InRecord->bWanted && Existing->bReady == InRecord->bReady &&
			Existing->bCommitted == InRecord->bCommitted && Existing->bHasGeometry == InRecord->bHasGeometry)
		{ OutError.Reset(); return true; }
		if (!Existing && ReservedRecordLimit > 0 && NumRecords() >= ReservedRecordLimit)
		{ OutError = TEXT("CapacityBlocked: CoverageIndex record capacity"); return false; }
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
		auto Attempt = Consumer;
		check(Serial < MAX_uint64);
		Attempt.RequestSerial = Serial + 1;
		const auto& Geometry = InRecord ? InRecord->Geometry : Existing->Geometry;
		Attempt.Kind = InKey.Representation == EVoxelTerrainRepresentation::Fine ?
			(InKey.Level == 0 ? EVoxelGenerationConsumerKind::F0Section : EVoxelGenerationConsumerKind::FineShard) :
			(Target.bColumn ? EVoxelGenerationConsumerKind::Heightfield : EVoxelGenerationConsumerKind::VoxelProxy);
		Attempt.SpatialKind = EVoxelGenerationSpatialKind::Bounds;
		Geometry.Key.TryBounds(Geometry.WorldMinZ, Geometry.WorldMaxZ, Attempt.Min, Attempt.Max);
		if (!Resources->Acquire(Attempt, EVoxelTerrainResourcePool::Input, Bytes, Grant, PathCount + (InRecord ? 1 : 0)))
		{ OutError = TEXT("CapacityBlocked: CoverageIndex changed path"); return false; }
	}
	const auto TakeLease = [&Grant, this](const uint64 Count)
	{
		return Resources ? Grant->Split(Count) : Grant;
	};
	if (InRecord && (Grant || !InSharedRecord))
	{
		const auto Lease = TakeLease(RecordBytes);
		InSharedRecord = Lease ? VoxelBoundedAllocation::MakeWorkerShared<FVoxelCoverageRecord>(Lease, *InRecord) :
			MakeShared<FVoxelCoverageRecord, ESPMode::ThreadSafe>(*InRecord);
	}
	auto Candidate = Grant ? VoxelBoundedAllocation::MakeWorkerShared<FRoot>(Grant) : MakeShared<FRoot, ESPMode::ThreadSafe>();
	if (Root) *Candidate = *Root;
	Candidate->Lease = Grant;
	Serial = NextVoxelCoverageVersion.FetchAdd(1) + 1;
	check(Serial != 0);
	const int32 Slot = RootSlot(Target);
	Candidate->Nodes[Slot] = MutateNode(Candidate->Nodes[Slot], RootKey(Slot), Target,
		RecordSlot, MoveTemp(InSharedRecord), bInputsChanged, Grant);
	if (bInputsChanged) Candidate->Versions[Slot][0] = Serial;
	Candidate->Versions[Slot][1] = Serial;
	Candidate->Records = Candidate->NodesCount = 0;
	for (const auto& Node : Candidate->Nodes)
		if (Node) { Candidate->Records += Node->SubtreeRecords; Candidate->NodesCount += Node->SubtreeNodes; }
	Root = MoveTemp(Candidate);
	if (bInputsChanged) MembershipVersion = Serial;
	OutError.Reset();
	return true;
}

FVoxelCoverageIndex::FNodePtr FVoxelCoverageIndex::MutateNode(FNodePtr InPrevious,
	const FVoxelCoverageIndexKey& InNodeKey, const FVoxelCoverageIndexKey& InTarget, const int32 InRecordSlot,
	TSharedPtr<const FVoxelCoverageRecord, ESPMode::ThreadSafe> InRecord, const bool bInInputsChanged,
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InGrant)
{
	const auto Lease = Resources ? InGrant->Split(VoxelBoundedAllocation::WorkerSharedBytes<FNode>()) : InGrant;
	auto Node = Lease ? VoxelBoundedAllocation::MakeWorkerShared<FNode>(Lease) : MakeShared<FNode, ESPMode::ThreadSafe>();
	if (InPrevious) *Node = *InPrevious;
	Node->Lease = Lease;
	if (InNodeKey.Level == InTarget.Level)
	{
		Node->Records[InRecordSlot] = MoveTemp(InRecord);
		if (bInInputsChanged) Node->RecordVersions[InRecordSlot][0] = Node->OwnVersions[0] = Serial;
		Node->RecordVersions[InRecordSlot][1] = Node->OwnVersions[1] = Serial;
	}
	else
	{
		const int32 Child = ChildSlot(InTarget, InNodeKey.Level);
		Node->Children[Child] = MutateNode(Node->Children[Child], ChildKey(InNodeKey, Child), InTarget,
			InRecordSlot, MoveTemp(InRecord), bInInputsChanged, InGrant);
		if (bInInputsChanged) Node->ChildVersions[Child][0] = Serial;
		Node->ChildVersions[Child][1] = Serial;
	}
	if (bInInputsChanged) Node->SubtreeVersions[0] = Serial;
	Node->SubtreeVersions[1] = Serial;
	Node->SubtreeRecords = 0; Node->SubtreeNodes = 1;
	for (const auto& Record : Node->Records) Node->SubtreeRecords += Record.IsValid();
	for (const auto& Child : Node->Children)
		if (Child) { Node->SubtreeRecords += Child->SubtreeRecords; Node->SubtreeNodes += Child->SubtreeNodes; }
	// 空路径立即退休；祖先槽保留单调 gap 版本，取消后的 ABA 不需要永久空节点。
	return Node->SubtreeRecords ? FNodePtr(Node) : nullptr;
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

bool FVoxelCoverageIndex::ReserveSnapshot(FVoxelCoverageLocalSnapshot& InOutSnapshot) const
{
	const uint64 Bytes =
		FMath::Max<uint64>(InOutSnapshot.Records.GetAllocatedSize(), VoxelBoundedAllocation::ArrayBytes<TSharedPtr<const FVoxelCoverageRecord, ESPMode::ThreadSafe>>(InOutSnapshot.MatchedRecords)) +
		FMath::Max<uint64>(InOutSnapshot.RecordGuards.GetAllocatedSize(), VoxelBoundedAllocation::ArrayBytes<FVoxelCoverageRecordReadGuard>(InOutSnapshot.MatchedRecords)) +
		FMath::Max<uint64>(InOutSnapshot.Guards.GetAllocatedSize(), VoxelBoundedAllocation::ArrayBytes<FVoxelCoverageReadGuard>(InOutSnapshot.GuardCount)) +
		FVoxelCoverageLocalSnapshot::ArrayMetadataBytes();
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> CandidateLease;
	if (Resources && Bytes)
	{
		auto Attempt = Consumer; Attempt.RequestSerial = FMath::Max<uint64>(1, Serial);
		if (!Resources->Acquire(Attempt, EVoxelTerrainResourcePool::Input, Bytes, CandidateLease)) return false;
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
	Root.Reset(); StorageLease.Reset(); ReservedRecordLimit = 0;
	// Serial 不回退，世界换代由 StorageGeneration 明确隔离。
	MembershipVersion = Serial;
	check(StorageGeneration < MAX_uint64); ++StorageGeneration;
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
	FVoxelCoverageLocalSnapshot& OutSnapshot, const bool bInIncludePresentation, const bool bInFullIndex)
{
	OutSnapshot = {};
	OutSnapshot.StorageId = InFrozen.StorageId;
	OutSnapshot.StorageGeneration = InFrozen.StorageGeneration; OutSnapshot.MembershipVersion = InFrozen.MembershipVersion;
	OutSnapshot.bFullIndex = bInFullIndex; OutSnapshot.bIncludePresentation = bInIncludePresentation; OutSnapshot.bCountOnly = true;
	if (InQuery.ExactMin.X >= InQuery.ExactMax.X || InQuery.ExactMin.Y >= InQuery.ExactMax.Y || InQuery.ExactMin.Z >= InQuery.ExactMax.Z) co_return;
	const int32 V = bInIncludePresentation ? 1 : 0;
	for (int32 Slot = 0; Slot < 12; ++Slot)
		co_await CaptureNodeSteps(InFrozen.Root ? InFrozen.Root->Nodes[Slot] : nullptr, RootKey(Slot), InQuery, OutSnapshot,
			InFrozen.Root ? InFrozen.Root->Versions[Slot][V] : 0);
	while (!InFrozen.ReserveSnapshot(OutSnapshot)) co_await FVoxelGameThreadContinuation::Yield();
	OutSnapshot.bCountOnly = false;
	OutSnapshot.VisitedIndexNodes = OutSnapshot.ExaminedRecords = OutSnapshot.MatchedRecords = OutSnapshot.GuardCount = 0;
	for (int32 Slot = 0; Slot < 12; ++Slot)
		co_await CaptureNodeSteps(InFrozen.Root ? InFrozen.Root->Nodes[Slot] : nullptr, RootKey(Slot), InQuery, OutSnapshot,
			InFrozen.Root ? InFrozen.Root->Versions[Slot][V] : 0);
}

FVoxelGameThreadContinuation FVoxelCoverageIndex::CaptureSteps(const FIntVector InMin, const FIntVector InMax,
	FVoxelCoverageLocalSnapshot& OutSnapshot, const bool bInIncludePresentation) const
{
	// 此函数本身不是 coroutine；调用时立即复制 O(1) 根，不能延迟至首次 Resume 才读 Current。
	return CaptureFrozenSteps(*this, Query(InMin, InMax), OutSnapshot, bInIncludePresentation, false);
}

FVoxelGameThreadContinuation FVoxelCoverageIndex::CaptureAllSteps(FVoxelCoverageLocalSnapshot& OutSnapshot, const bool bInIncludePresentation) const
{
	return CaptureFrozenSteps(*this, Query(FIntVector(MIN_int32), FIntVector(MAX_int32)), OutSnapshot, bInIncludePresentation, true);
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
			while (!InFrozen.ReserveSnapshot(OutSnapshot)) co_await FVoxelGameThreadContinuation::Yield();
			OutSnapshot.bCountOnly = false;
		}
	}
}

FVoxelGameThreadContinuation FVoxelCoverageIndex::CaptureChangedSteps(const FVoxelCoverageIndex& InPrevious,
	FVoxelCoverageLocalSnapshot& OutSnapshot) const
{
	return CaptureChangedFrozenSteps(*this, InPrevious, OutSnapshot);
}

#include "Voxel/Rendering/VoxelCoverageIndex.h"

FVoxelCoverageIndexKey FVoxelCoverageIndex::Parent(const FVoxelCoverageIndexKey& InKey)
{
	return {FIntPoint(static_cast<int32>(VoxelStreamingGeometry::FloorDivide(InKey.Coordinate.X, 2)),
		static_cast<int32>(VoxelStreamingGeometry::FloorDivide(InKey.Coordinate.Y, 2))), uint8(InKey.Level + 1)};
}

void FVoxelCoverageIndex::Touch(const FVoxelCoverageIndexKey& InKey, const int32 InCountDelta, const bool bInInputsChanged)
{
	check(Serial != MAX_uint64);
	const uint64 Version = ++Serial;
	FVoxelCoverageIndexKey Key = InKey;
	while (true)
	{
		FNode& Node = Nodes.FindOrAdd(Key);
		Node.SubtreeVersion = Version;
		if (bInInputsChanged) Node.SubtreeInputVersion = Version;
		Node.SubtreeRecords += InCountDelta;
		check(Node.SubtreeRecords >= 0);
		if (Key == InKey)
		{
			Node.OwnVersion = Version;
			if (bInInputsChanged) Node.OwnInputVersion = Version;
		}
		if (Key.Level == VoxelStreamingGeometry::MaximumSpatialLevel)
		{
			break;
		}
		Key = Parent(Key);
	}
}

bool FVoxelCoverageIndex::Upsert(TSharedPtr<const FVoxelCoverageRecord, ESPMode::ThreadSafe> InRecord,
	FString& OutError)
{
	if (!InRecord || !InRecord->Geometry.Validate(OutError))
	{
		if (!InRecord)
		{
			OutError = TEXT("Coverage record is null");
		}
		return false;
	}
	FIntVector Min;
	FIntVector Max;
	InRecord->Geometry.Key.TryBounds(InRecord->Geometry.WorldMinZ, InRecord->Geometry.WorldMaxZ, Min, Max);
	if (InRecord->Min != Min || InRecord->Max != Max)
	{
		OutError = TEXT("Index records must use their exact dyadic key bounds; clipped ownership is stored in the ledger");
		return false;
	}
	const FVoxelTerrainSpatialKey Key = InRecord->Geometry.Key;
	const auto* Existing = Records.Find(Key);
	if (Existing && (*Existing)->Geometry == InRecord->Geometry && (*Existing)->Stamp == InRecord->Stamp &&
		(*Existing)->bWanted == InRecord->bWanted && (*Existing)->bReady == InRecord->bReady &&
		(*Existing)->bCommitted == InRecord->bCommitted && (*Existing)->bHasGeometry == InRecord->bHasGeometry)
	{
		OutError.Reset();
		return true;
	}
	const bool bNew = Existing == nullptr;
	const bool bInputsChanged = bNew || (*Existing)->Geometry != InRecord->Geometry ||
		(*Existing)->Stamp != InRecord->Stamp || (*Existing)->bWanted != InRecord->bWanted ||
		(*Existing)->bReady != InRecord->bReady || (*Existing)->bHasGeometry != InRecord->bHasGeometry;
	Records.Add(Key, MoveTemp(InRecord));
	const FVoxelCoverageIndexKey NodeKey{FIntPoint(Key.Coordinate.X, Key.Coordinate.Y), Key.Level};
	Nodes.FindOrAdd(NodeKey).Records.Add(Key);
	Touch(NodeKey, bNew ? 1 : 0, bInputsChanged);
	OutError.Reset();
	return true;
}

bool FVoxelCoverageIndex::Remove(const FVoxelTerrainSpatialKey& InKey)
{
	if (Records.Remove(InKey) == 0)
	{
		return false;
	}
	const FVoxelCoverageIndexKey NodeKey{FIntPoint(InKey.Coordinate.X, InKey.Coordinate.Y), InKey.Level};
	Nodes.FindChecked(NodeKey).Records.Remove(InKey);
	Touch(NodeKey, -1);
	return true;
}

void FVoxelCoverageIndex::AppendOwn(const FNode& InNode, const FQuery& InQuery,
	FVoxelCoverageLocalSnapshot& OutSnapshot) const
{
	for (const FVoxelTerrainSpatialKey& Key : InNode.Records)
	{
		++OutSnapshot.ExaminedRecords;
		const auto& Record = Records.FindChecked(Key);
		bool bIntersects = true;
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			bIntersects &= Record->Min[Axis] < InQuery.ExactMax[Axis] && Record->Max[Axis] > InQuery.ExactMin[Axis];
		}
		if (bIntersects)
		{
			OutSnapshot.Records.Add(Record);
		}
	}
}

void FVoxelCoverageIndex::CollectSubtree(const FVoxelCoverageIndexKey& InKey, const FQuery& InQuery,
	FVoxelCoverageLocalSnapshot& OutSnapshot) const
{
	const FNode* Node = Nodes.Find(InKey);
	if (!Node || Node->SubtreeRecords == 0)
	{
		return;
	}
	++OutSnapshot.VisitedIndexNodes;
	AppendOwn(*Node, InQuery, OutSnapshot);
	if (InKey.Level == 0)
	{
		return;
	}
	for (int32 Child = 0; Child < 4; ++Child)
	{
		CollectSubtree({InKey.Coordinate * 2 + FIntPoint(Child & 1, (Child >> 1) & 1),
			uint8(InKey.Level - 1)}, InQuery, OutSnapshot);
	}
}

void FVoxelCoverageIndex::CaptureNode(const FVoxelCoverageIndexKey& InKey, const FQuery& InQuery,
	FVoxelCoverageLocalSnapshot& OutSnapshot) const
{
	const int64 Side = int64(16) << InKey.Level;
	const int64 MinX = int64(InKey.Coordinate.X) * Side;
	const int64 MinY = int64(InKey.Coordinate.Y) * Side;
	const int64 MaxX = MinX + Side;
	const int64 MaxY = MinY + Side;
	if (MinX >= InQuery.MaxX || MaxX <= InQuery.MinX || MinY >= InQuery.MaxY || MaxY <= InQuery.MinY)
	{
		return;
	}
	const FNode* Node = Nodes.Find(InKey);
	const bool bCovered = InQuery.MinX <= MinX && InQuery.MinY <= MinY &&
		InQuery.MaxX >= MaxX && InQuery.MaxY >= MaxY;
	if (bCovered || InKey.Level == 0)
	{
		OutSnapshot.Guards.Add({InKey, Node ? (OutSnapshot.bIncludePresentation ? Node->SubtreeVersion : Node->SubtreeInputVersion) : 0, true});
		CollectSubtree(InKey, InQuery, OutSnapshot);
		return;
	}
	++OutSnapshot.VisitedIndexNodes;
	OutSnapshot.Guards.Add({InKey, Node ? (OutSnapshot.bIncludePresentation ? Node->OwnVersion : Node->OwnInputVersion) : 0, false});
	if (Node)
	{
		AppendOwn(*Node, InQuery, OutSnapshot);
	}
	for (int32 Child = 0; Child < 4; ++Child)
	{
		CaptureNode({InKey.Coordinate * 2 + FIntPoint(Child & 1, (Child >> 1) & 1),
			uint8(InKey.Level - 1)}, InQuery, OutSnapshot);
	}
}

void FVoxelCoverageIndex::Capture(const FIntVector& InMin, const FIntVector& InMax,
	FVoxelCoverageLocalSnapshot& OutSnapshot, const bool bInIncludePresentation) const
{
	OutSnapshot = {};
	OutSnapshot.StorageGeneration = StorageGeneration;
	OutSnapshot.bIncludePresentation = bInIncludePresentation;
	if (InMin.X >= InMax.X || InMin.Y >= InMax.Y || InMin.Z >= InMax.Z)
	{
		return;
	}
	FQuery Query;
	Query.MinX = VoxelStreamingGeometry::FloorDivide(InMin.X, 16) * 16;
	Query.MinY = VoxelStreamingGeometry::FloorDivide(InMin.Y, 16) * 16;
	Query.MaxX = (VoxelStreamingGeometry::FloorDivide(int64(InMax.X) - 1, 16) + 1) * 16;
	Query.MaxY = (VoxelStreamingGeometry::FloorDivide(int64(InMax.Y) - 1, 16) + 1) * 16;
	Query.ExactMin = InMin;
	Query.ExactMax = InMax;
	const int64 RootSide = int64(16) << VoxelStreamingGeometry::MaximumSpatialLevel;
	for (int64 Y = VoxelStreamingGeometry::FloorDivide(Query.MinY, RootSide);
		Y <= VoxelStreamingGeometry::FloorDivide(Query.MaxY - 1, RootSide); ++Y)
	{
		for (int64 X = VoxelStreamingGeometry::FloorDivide(Query.MinX, RootSide);
			X <= VoxelStreamingGeometry::FloorDivide(Query.MaxX - 1, RootSide); ++X)
		{
			CaptureNode({FIntPoint(static_cast<int32>(X), static_cast<int32>(Y)),
				VoxelStreamingGeometry::MaximumSpatialLevel}, Query, OutSnapshot);
		}
	}
}

bool FVoxelCoverageIndex::IsCurrent(const FVoxelCoverageLocalSnapshot& InSnapshot) const
{
	if (InSnapshot.StorageGeneration != StorageGeneration)
	{
		return false;
	}
	for (const FVoxelCoverageReadGuard& Guard : InSnapshot.Guards)
	{
		const FNode* Node = Nodes.Find(Guard.Key);
		const uint64 Current = !Node ? 0 : InSnapshot.bIncludePresentation
			? (Guard.bSubtree ? Node->SubtreeVersion : Node->OwnVersion)
			: (Guard.bSubtree ? Node->SubtreeInputVersion : Node->OwnInputVersion);
		if (Current != Guard.Version)
		{
			return false;
		}
	}
	return true;
}

void FVoxelCoverageIndex::PruneEmptyNodes(const int32 InOutstandingSnapshots)
{
	check(InOutstandingSnapshots == 0);
	for (auto Iterator = Nodes.CreateIterator(); Iterator; ++Iterator)
	{
		if (Iterator.Value().SubtreeRecords == 0)
		{
			Iterator.RemoveCurrent();
		}
	}
	check(StorageGeneration != MAX_uint64);
	++StorageGeneration;
}

void FVoxelCoverageIndex::Reset()
{
	Records.Reset();
	Nodes.Reset();
	Serial = 0;
	check(StorageGeneration != MAX_uint64);
	++StorageGeneration;
}

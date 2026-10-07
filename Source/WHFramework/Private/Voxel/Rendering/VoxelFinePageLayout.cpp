#include "Voxel/Rendering/VoxelFinePageLayout.h"

bool FVoxelFinePageLayout::Initialize(const uint8 InMaximumLevel, const int32 InShardSideCells,
	const int32 InFirstMergeDistanceCells, FString& OutError)
{
	if (InMaximumLevel < 1 || InMaximumLevel > 4 || InShardSideCells < 32 || InShardSideCells > 64 ||
		!FMath::IsPowerOfTwo(InShardSideCells) || InShardSideCells > (16 << InMaximumLevel) ||
		InFirstMergeDistanceCells < InShardSideCells || InFirstMergeDistanceCells > (16 << InMaximumLevel))
	{
		OutError = TEXT("Fine Page 布局配置不合法");
		return false;
	}
	Reset();
	MaximumLevel = InMaximumLevel;
	ShardSideCells = InShardSideCells;
	MinimumLevel = uint8(FMath::FloorLog2(uint32(InShardSideCells / 16)));
	FirstMergeDistanceCells = InFirstMergeDistanceCells;
	OutError.Reset();
	return true;
}

FVoxelFinePageKey FVoxelFinePageLayout::RootFor(const FVoxelFineShardKey& InShard) const
{
	const int32 Side = 16 << MaximumLevel;
	return {FIntVector(int32(FVoxelFineSpatial::FloorDivide(InShard.Min.X, Side)),
		int32(FVoxelFineSpatial::FloorDivide(InShard.Min.Y, Side)),
		int32(FVoxelFineSpatial::FloorDivide(InShard.Min.Z, Side))), MaximumLevel};
}

void FVoxelFinePageLayout::DirtyRoot(const FVoxelFinePageKey& InRoot)
{
	if (!QueuedRoots.Contains(InRoot))
	{
		QueuedRoots.Add(InRoot);
		DirtyQueue.Add(InRoot);
	}
}

void FVoxelFinePageLayout::DirtyInfluence(const FVoxelFineLevelAnchors& InAnchors)
{
	const int32 RootSide = 16 << MaximumLevel;
	for (uint8 Level = MinimumLevel + 1; Level <= MaximumLevel; ++Level)
	{
		if (!InAnchors.Levels.IsValidIndex(Level))
		{
			continue;
		}
		const int64 Radius = int64(FirstMergeDistanceCells) << (Level - MinimumLevel - 1);
		const FIntVector Anchor = InAnchors.Levels[Level];
		const int64 FirstX = FVoxelFineSpatial::FloorDivide(int64(Anchor.X) - Radius, RootSide);
		const int64 LastX = FVoxelFineSpatial::FloorDivide(int64(Anchor.X) + Radius, RootSide);
		const int64 FirstY = FVoxelFineSpatial::FloorDivide(int64(Anchor.Y) - Radius, RootSide);
		const int64 LastY = FVoxelFineSpatial::FloorDivide(int64(Anchor.Y) + Radius, RootSide);
		const int64 FirstZ = FVoxelFineSpatial::FloorDivide(int64(Anchor.Z) - Radius, RootSide);
		const int64 LastZ = FVoxelFineSpatial::FloorDivide(int64(Anchor.Z) + Radius, RootSide);
		for (int64 RootZ = FirstZ; RootZ <= LastZ; ++RootZ)
		{
			for (int64 RootY = FirstY; RootY <= LastY; ++RootY)
			{
				for (int64 RootX = FirstX; RootX <= LastX; ++RootX)
				{
					const FVoxelFinePageKey Key{FIntVector(int32(RootX), int32(RootY), int32(RootZ)), MaximumLevel};
					if (RootShards.Contains(Key))
					{
						DirtyRoot(Key);
					}
				}
			}
		}
	}
}

bool FVoxelFinePageLayout::SetSource(const FGuid& InSource, const FIntVector& InPosition, FString& OutError)
{
	if (!InSource.IsValid())
	{
		OutError = TEXT("Fine Level Anchor 缺少稳定 Source 身份");
		return false;
	}
	if (Anchors.Num() == MAX_int32 || !ReserveSource(Anchors.Num() + 1, OutError)) return false;
	FVoxelFineLevelAnchors Candidate;
	Candidate.Source = InSource;
	Candidate.Levels.Reserve(MaximumLevel + 1);
	Candidate.Levels.SetNum(MaximumLevel + 1);
	for (uint8 Level = 0; Level <= MaximumLevel; ++Level)
	{
		if (!FVoxelFineSpatial::TrySnap(InPosition, 16 << Level, Candidate.Levels[Level]))
		{
			OutError = TEXT("Fine Level Anchor 坐标越界");
			return false;
		}
	}
	const FVoxelFineLevelAnchors* Previous = Anchors.Find(InSource);
	bool bChanged = !Previous;
	if (Previous)
	{
		for (uint8 Level = MinimumLevel + 1; Level <= MaximumLevel; ++Level)
		{
			bChanged |= Previous->Levels[Level] != Candidate.Levels[Level];
		}
		if (bChanged)
		{
			DirtyInfluence(*Previous);
		}
	}
	if (bChanged)
	{
		DirtyInfluence(Candidate);
	}
	Anchors.Add(InSource, MoveTemp(Candidate));
	SettleStorage();
	OutError.Reset();
	return true;
}

bool FVoxelFinePageLayout::RemoveSource(const FGuid& InSource, FString& OutError)
{
	if (!ReserveSource(Anchors.Num(), OutError)) return false;
	if (const FVoxelFineLevelAnchors* Previous = Anchors.Find(InSource))
	{
		DirtyInfluence(*Previous);
		Anchors.Remove(InSource);
	}
	SettleStorage();
	OutError.Reset();
	return true;
}

bool FVoxelFinePageLayout::ApplyResidencyDelta(const TVoxelFineSetDelta<FVoxelFineShardKey>& InDelta, FString& OutError)
{
	for (const FVoxelFineShardKey& Key : InDelta.Added)
	{
		if (!Key.Validate(OutError) || Key.SideCells != ShardSideCells)
		{
			OutError = TEXT("Page 布局只接受规范 Shard；复杂度子分片属于同一规范 Shard 内部");
			return false;
		}
	}
	for (const FVoxelFineShardKey& Key : InDelta.Removed)
	{
		if (!Key.Validate(OutError) || Key.SideCells != ShardSideCells)
		{
			return false;
		}
	}
	if (!ReserveResidencyDelta(InDelta, OutError)) return false;
	for (const FVoxelFineShardKey& Key : InDelta.Added)
	{
		const FVoxelFinePageKey Root = RootFor(Key);
		RootShards.FindOrAdd(Root).Add(Key);
		DirtyRoot(Root);
	}
	for (const FVoxelFineShardKey& Key : InDelta.Removed)
	{
		const FVoxelFinePageKey Root = RootFor(Key);
		if (TSet<FVoxelFineShardKey>* Shards = RootShards.Find(Root))
		{
			Shards->Remove(Key);
			if (Shards->IsEmpty())
			{
				RootShards.Remove(Root);
			}
		}
		DirtyRoot(Root);
	}
	SettleStorage();
	OutError.Reset();
	return true;
}

bool FVoxelFinePageLayout::CanMerge(const FVoxelFinePageKey& InParent) const
{
	FVoxelGenerationBounds Bounds;
	const int64 Side = int64(16) << InParent.Level;
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		const int64 Minimum = int64(InParent.Coordinate[Axis]) * Side;
		const int64 Maximum = Minimum + Side;
		if (Minimum < MIN_int32 || Maximum > MAX_int32)
		{
			return false;
		}
		Bounds.Min[Axis] = int32(Minimum);
		Bounds.Max[Axis] = int32(Maximum);
	}
	const double RequiredDistance = double(int64(FirstMergeDistanceCells) << (InParent.Level - MinimumLevel - 1));
	for (const TPair<FGuid, FVoxelFineLevelAnchors>& Pair : Anchors)
	{
		if (FVoxelFineSpatial::DistanceToBounds(FVector(Pair.Value.Levels[InParent.Level]), Bounds) < RequiredDistance)
		{
			return false;
		}
	}
	return true;
}

void FVoxelFinePageLayout::BuildRoot(const FVoxelFinePageKey& InRoot,
	TMap<FVoxelFinePageKey, FVoxelFineLayoutPage>& OutPages) const
{
	OutPages.Reset();
	const TSet<FVoxelFineShardKey>* Shards = RootShards.Find(InRoot);
	if (!Shards)
	{
		return;
	}
	OutPages.Reserve(Shards->Num());
	for (const FVoxelFineShardKey& Shard : *Shards)
	{
		const FVoxelFinePageKey Key{Shard.Min / ShardSideCells, MinimumLevel};
		FVoxelFineLayoutPage Page;
		Page.Key = Key;
		Page.Shards.Reserve(1);
		Page.Shards.Add(Shard);
		OutPages.Add(Key, MoveTemp(Page));
	}
	for (uint8 Level = MinimumLevel + 1; Level <= MaximumLevel; ++Level)
	{
		TSet<FVoxelFinePageKey> Parents;
		Parents.Reserve(Shards->Num());
		for (const TPair<FVoxelFinePageKey, FVoxelFineLayoutPage>& Pair : OutPages)
		{
			if (Pair.Key.Level == Level - 1)
			{
				Parents.Add({FIntVector(int32(FVoxelFineSpatial::FloorDivide(Pair.Key.Coordinate.X, 2)),
					int32(FVoxelFineSpatial::FloorDivide(Pair.Key.Coordinate.Y, 2)),
					int32(FVoxelFineSpatial::FloorDivide(Pair.Key.Coordinate.Z, 2))), Level});
			}
		}
		for (const FVoxelFinePageKey& Parent : Parents)
		{
			if (!CanMerge(Parent))
			{
				continue;
			}
			TArray<FVoxelFinePageKey, TInlineAllocator<8>> Children;
			for (int32 Child = 0; Child < 8; ++Child)
			{
				const FVoxelFinePageKey Key{Parent.Coordinate * 2 +
					FIntVector(Child & 1, (Child >> 1) & 1, (Child >> 2) & 1), uint8(Level - 1)};
				if (!OutPages.Contains(Key))
				{
					break;
				}
				Children.Add(Key);
			}
			if (Children.Num() != 8)
			{
				continue;
			}
			FVoxelFineLayoutPage Page;
			Page.Key = Parent;
			int32 ShardCount = 0;
			for (const auto& Child : Children) ShardCount += OutPages.FindChecked(Child).Shards.Num();
			Page.Shards.Reserve(ShardCount);
			for (const FVoxelFinePageKey& Child : Children)
			{
				Page.Shards.Append(OutPages.FindChecked(Child).Shards);
				OutPages.Remove(Child);
			}
			OutPages.Add(Parent, MoveTemp(Page));
		}
	}
}

int32 FVoxelFinePageLayout::Advance(const int32 InMaximumRoots, FVoxelFinePageLayoutDelta& OutDelta,
	FString* OutError, const bool bInProduceDelta)
{
	OutDelta = {};
	FString Error;
	if (OutError) OutError->Reset();
	int32 Processed = 0;
	while (Processed < FMath::Max(0, InMaximumRoots) && DirtyQueue.IsValidIndex(DirtyCursor))
	{
		const FVoxelFinePageKey Root = DirtyQueue[DirtyCursor];
		const auto* Shards = RootShards.Find(Root);
		const int32 ShardCount = Shards ? Shards->Num() : 0;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Scratch;
		if (!ReserveAdvance(Root, ShardCount, OutDelta, bInProduceDelta, Scratch, Error))
		{
			if (OutError) *OutError = MoveTemp(Error);
			return Processed;
		}
		++DirtyCursor;
		QueuedRoots.Remove(Root);
		TMap<FVoxelFinePageKey, FVoxelFineLayoutPage> Next;
		BuildRoot(Root, Next);
		if (const TSet<FVoxelFinePageKey>* Previous = RootPages.Find(Root))
		{
			for (const FVoxelFinePageKey& Key : *Previous)
			{
				if (!Next.Contains(Key))
				{
					OutDelta.bChanged = true;
					if (bInProduceDelta) OutDelta.Removed.Add(Key);
					Pages.Remove(Key);
				}
			}
		}
		TSet<FVoxelFinePageKey> NewKeys;
		NewKeys.Reserve(ShardCount);
		for (TPair<FVoxelFinePageKey, FVoxelFineLayoutPage>& Pair : Next)
		{
			NewKeys.Add(Pair.Key);
			if (Pages.Contains(Pair.Key))
			{
				if (bInProduceDelta) OutDelta.Retained.Add(Pair.Key);
			}
			else
			{
				OutDelta.bChanged = true;
				if (bInProduceDelta) OutDelta.Added.Add(Pair.Value);
				Pages.Add(Pair.Key, MoveTemp(Pair.Value));
			}
		}
		if (NewKeys.IsEmpty())
		{
			RootPages.Remove(Root);
		}
		else
		{
			RootPages.Add(Root, MoveTemp(NewKeys));
		}
		++Processed;
		SettleStorage();
	}
	if (DirtyCursor == DirtyQueue.Num())
	{
		DirtyQueue.Reset();
		DirtyCursor = 0;
	}
	return Processed;
}

const TMap<FVoxelFinePageKey, FVoxelFineLayoutPage>& FVoxelFinePageLayout::GetPages() const
{
	return Pages;
}

const TMap<FGuid, FVoxelFineLevelAnchors>& FVoxelFinePageLayout::GetAnchors() const
{
	return Anchors;
}

bool FVoxelFinePageLayout::IsPending() const
{
	return DirtyCursor < DirtyQueue.Num();
}

void FVoxelFinePageLayout::Reset()
{
	Anchors.Empty();
	RootShards.Empty();
	Pages.Empty();
	RootPages.Empty();
	DirtyQueue.Empty();
	QueuedRoots.Empty();
	DirtyCursor = 0;
	SettleStorage();
}

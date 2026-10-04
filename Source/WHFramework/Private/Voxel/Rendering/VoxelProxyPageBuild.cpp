#include "Voxel/Rendering/VoxelProxyPageBuild.h"

bool FVoxelProxyPageBuild::Initialize(const FVoxelTerrainGeometry& InGeometry, FString& OutError)
{
	Geometry = InGeometry;
	if (!VoxelStreamingGeometry::BuildShards(Geometry, 16, 4096, Shards, OutError)) return false;
	// The final packed page and immutable execution windows must fit without changing requested sampling.
	if (MaximumRetainedBytes() > 24ull * 1024ull * 1024ull)
	{ OutError = TEXT("Oversized: proxy page completion exceeds retained-input capacity"); return false; }
	return true;
}

uint64 FVoxelProxyPageBuild::MaximumRetainedBytes() const
{
	const uint64 Side = Geometry.Key.SideCells() / FMath::Max(1, Geometry.SampleStepCells);
	return 64ull * 1024ull + (2 * Side * Side * Side + 6 * Side * Side + uint64(Shards.Num()) * 6 * 16 * 16) * sizeof(FVoxelBlockState);
}

bool FVoxelProxyPageBuild::Advance(const FVoxelVoxelProxyBuilder& InBuilder,
	const FVoxelOverlaySnapshotSet& InOverlays, FVoxelVoxelProxyData& OutData,
	bool& bOutComplete, FString& OutError, const TAtomic<bool>* InCancel)
{
	bOutComplete = false;
	const FVoxelViewKey Key{Geometry.Key.Coordinate, Geometry.Key.Level};
	if (Shards.IsValidIndex(NextShard))
	{
		FVoxelVoxelProxyData Part;
		if (!InBuilder.Build(Key, InOverlays, Part, OutError, InCancel, Geometry, &Shards[NextShard])) return false;
		Completed.Add(MoveTemp(Part));
		++NextShard;
		// Return to the original scheduler after each bounded 16-cubed generation window.
		return true;
	}
	const int32 Side = Geometry.Key.SideCells() / Geometry.SampleStepCells;
	FVoxelVoxelProxyData Page;
	Page.Key = Key;
	Page.Geometry = Geometry;
	Page.GridSide = Side;
	Page.Cells.SetNumUninitialized(Side * Side * Side);
	for (int32 Face = 0; Face < 6; ++Face) { Page.Halo[Face].SetNumUninitialized(Side * Side); Page.Known[Face] = true; }
	for (const auto& Part : Completed)
	{
		if (InCancel && InCancel->Load()) { OutError = TEXT("Canceled"); return false; }
		const FIntVector Offset = Part.LocalMinCells / Geometry.SampleStepCells;
		const int32 Count = Part.GridSide;
		for (int32 Z = 0; Z < Count; ++Z)
			for (int32 Y = 0; Y < Count; ++Y)
				for (int32 X = 0; X < Count; ++X)
					Page.Cells[Offset.X + X + Side * (Offset.Y + Y + Side * (Offset.Z + Z))] = Part.Cells[X + Count * (Y + Count * Z)];
		for (int32 Face = 0; Face < 6; ++Face)
		{
			const int32 Axis = Face / 2, U = (Axis + 1) % 3, V = (Axis + 2) % 3;
			if ((Face & 1) ? Offset[Axis] != 0 : Offset[Axis] + Count != Side) continue;
			Page.Known[Face] &= Part.Known[Face];
			for (int32 Y = 0; Y < Count; ++Y)
				for (int32 X = 0; X < Count; ++X)
					Page.Halo[Face][Offset[U] + X + Side * (Offset[V] + Y)] = Part.Halo[Face][X + Count * Y];
		}
		Page.bHasVisibleSurfaceEvidence |= Part.bHasVisibleSurfaceEvidence;
	}
	Completed.Reset();
	OutData = MoveTemp(Page);
	bOutComplete = true;
	return true;
}

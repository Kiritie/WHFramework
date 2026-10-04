#include "Voxel/Rendering/VoxelHeightfieldPageBuild.h"

namespace
{
	template<typename T>
	void CopyGrid(const TArray<T>& InSource, int32 InSide, TArray<T>& OutTarget, int32 OutSide, FIntPoint Offset)
	{
		for (int32 Y = 0; Y < InSide; ++Y)
			for (int32 X = 0; X < InSide; ++X)
				OutTarget[Offset.X + X + OutSide * (Offset.Y + Y)] = InSource[X + InSide * Y];
	}
}

bool FVoxelHeightfieldPageBuild::Initialize(const FVoxelTerrainGeometry& InGeometry, FString& OutError)
{
	Geometry = InGeometry;
	if (!VoxelStreamingGeometry::BuildShards(Geometry, 64, 4096, Shards, OutError)) return false;
	if (MaximumRetainedBytes() > 24ull * 1024ull * 1024ull)
	{ OutError = TEXT("Oversized: heightfield page completion exceeds retained-input capacity"); return false; }
	return true;
}

uint64 FVoxelHeightfieldPageBuild::MaximumRetainedBytes() const
{
	const uint64 Side = Geometry.Key.SideCells() / FMath::Max(1, Geometry.SampleStepCells) + 1;
	return 64ull * 1024ull + 48 * Side * Side;
}

bool FVoxelHeightfieldPageBuild::Advance(const FVoxelSurfaceProxyBuilder& InBuilder,
	FVoxelSurfaceTileData& OutData, bool& bOutComplete, FString& OutError, const TAtomic<bool>* InCancel)
{
	bOutComplete = false;
	const FVoxelSurfaceTileKey Key{FIntPoint(Geometry.Key.Coordinate.X, Geometry.Key.Coordinate.Y), Geometry.Key.Level};
	if (Shards.IsValidIndex(NextShard))
	{
		FVoxelSurfaceTileData Part;
		if (!InBuilder.Build(Key, Part, OutError, InCancel, nullptr, nullptr, Geometry, &Shards[NextShard])) return false;
		SurfaceParts.Add(MoveTemp(Part));
		++NextShard;
		return true;
	}
	FVoxelSurfaceTileData Page;
	Page.Key = Key;
	Page.Geometry = Geometry;
	Page.Step = Geometry.SampleStepCells;
	Page.Side = Geometry.Key.SideCells() / Page.Step + 1;
	Page.GroundZ.SetNumUninitialized(Page.Side * Page.Side);
	Page.WaterZ.SetNumUninitialized(Page.Side * Page.Side);
	Page.SurfaceMaterial.SetNumUninitialized(Page.Side * Page.Side);
	Page.Biome.SetNumUninitialized(Page.Side * Page.Side);
	Page.Flags.SetNumUninitialized(Page.Side * Page.Side);
	for (const auto& Part : SurfaceParts)
	{
		if (InCancel && InCancel->Load()) { OutError = TEXT("Canceled"); return false; }
		const FIntPoint Offset = Part.LocalMinCells / Page.Step;
		CopyGrid(Part.GroundZ, Part.Side, Page.GroundZ, Page.Side, Offset);
		CopyGrid(Part.WaterZ, Part.Side, Page.WaterZ, Page.Side, Offset);
		CopyGrid(Part.SurfaceMaterial, Part.Side, Page.SurfaceMaterial, Page.Side, Offset);
		CopyGrid(Part.Biome, Part.Side, Page.Biome, Page.Side, Offset);
		CopyGrid(Part.Flags, Part.Side, Page.Flags, Page.Side, Offset);
		Page.Revision = FMath::Max(Page.Revision, Part.Revision);
		Page.DistantCells.Append(Part.DistantCells);
	}
	SurfaceParts.Reset();
	OutData = MoveTemp(Page);
	bOutComplete = true;
	return true;
}

bool FVoxelHeightfieldPageBuild::Advance(const FVoxelMacroTerrainBuilder& InBuilder,
	FVoxelMacroTileData& OutData, bool& bOutComplete, FString& OutError, const TAtomic<bool>* InCancel)
{
	bOutComplete = false;
	const FVoxelMacroTileKey Key{FIntPoint(Geometry.Key.Coordinate.X, Geometry.Key.Coordinate.Y), Geometry.Key.Level};
	if (Shards.IsValidIndex(NextShard))
	{
		FVoxelMacroTileData Part;
		if (!InBuilder.Build(Key, Part, OutError, InCancel, Geometry, &Shards[NextShard])) return false;
		MacroParts.Add(MoveTemp(Part));
		++NextShard;
		return true;
	}
	FVoxelMacroTileData Page;
	Page.Key = Key;
	Page.Geometry = Geometry;
	Page.Step = Geometry.SampleStepCells;
	Page.Side = Geometry.Key.SideCells() / Page.Step + 1;
	Page.Height.SetNumUninitialized(Page.Side * Page.Side);
	Page.WaterHeight.SetNumUninitialized(Page.Side * Page.Side);
	Page.WaterKind.SetNumUninitialized(Page.Side * Page.Side);
	Page.SurfaceClass.SetNumUninitialized(Page.Side * Page.Side);
	Page.ForestCoverage.SetNumUninitialized(Page.Side * Page.Side);
	Page.SnowCoverage.SetNumUninitialized(Page.Side * Page.Side);
	for (const auto& Part : MacroParts)
	{
		if (InCancel && InCancel->Load()) { OutError = TEXT("Canceled"); return false; }
		const FIntPoint Offset = Part.LocalMinCells / Page.Step;
		CopyGrid(Part.Height, Part.Side, Page.Height, Page.Side, Offset);
		CopyGrid(Part.WaterHeight, Part.Side, Page.WaterHeight, Page.Side, Offset);
		CopyGrid(Part.WaterKind, Part.Side, Page.WaterKind, Page.Side, Offset);
		CopyGrid(Part.SurfaceClass, Part.Side, Page.SurfaceClass, Page.Side, Offset);
		CopyGrid(Part.ForestCoverage, Part.Side, Page.ForestCoverage, Page.Side, Offset);
		CopyGrid(Part.SnowCoverage, Part.Side, Page.SnowCoverage, Page.Side, Offset);
		Page.Revision = FMath::Max(Page.Revision, Part.Revision);
		Page.DistantCells.Append(Part.DistantCells);
	}
	MacroParts.Reset();
	OutData = MoveTemp(Page);
	bOutComplete = true;
	return true;
}

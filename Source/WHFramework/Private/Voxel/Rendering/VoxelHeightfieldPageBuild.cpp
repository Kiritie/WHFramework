#include "Voxel/Rendering/VoxelHeightfieldPageBuild.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"

namespace
{
	constexpr int32 GenerationSamples = 32;
	constexpr int32 MaximumWindowDistantCells = 16384;
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
	if (!InGeometry.Validate(OutError))
	{
		return false;
	}
	if (!InGeometry.Key.IsColumn())
	{
		OutError = TEXT("Heightfield page requires an explicit Surface or Macro geometry");
		return false;
	}
	FVoxelHeightfieldPageBuild Candidate;
	Candidate.Geometry = InGeometry;
	if (Candidate.MaximumRetainedBytes() > 24ull * 1024ull * 1024ull)
	{
		OutError = TEXT("Oversized: heightfield page completion exceeds retained-input capacity");
		return false;
	}
	if (!VoxelStreamingGeometry::BuildShards(InGeometry, GenerationSamples, 4096, Candidate.Shards, OutError))
	{
		return false;
	}
	*this = MoveTemp(Candidate);
	return true;
}

uint64 FVoxelHeightfieldPageBuild::MaximumRetainedBytes() const
{
	using namespace VoxelBoundedAllocation;
	const int64 Cells = Geometry.Key.SideCells() / FMath::Max(1, Geometry.SampleStepCells);
	if (Cells <= 0 || Cells > 256) return MAX_uint64;
	const int32 Side = int32(Cells + 1), Parts = int32((Cells + GenerationSamples - 1) / GenerationSamples);
	const int32 Count = Side * Side;
	// Surface + Water 和 Macro 共用较大的 Surface 输出容量模型。
	return WorkerSharedBytes<FVoxelHeightfieldPageBuild>() + ArrayBytes<FVoxelTerrainBuildShard>(Parts * Parts) +
		4 * ArrayBytes<int32>(Count) + ArrayBytes<FVoxelBlockState>(Count) + ArrayBytes<uint16>(Count) + 2 * ArrayBytes<uint8>(Count) +
		MaximumDistantBytes();
}

uint64 FVoxelHeightfieldPageBuild::MaximumDistantBytes() const
{
	const int64 Cells = Geometry.Key.SideCells() / FMath::Max(1, Geometry.SampleStepCells);
	if (Cells <= 0 || Cells > 256)
	{
		return MAX_uint64;
	}
	const int32 Parts = int32((Cells + GenerationSamples - 1) / GenerationSamples);
	return VoxelBoundedAllocation::ArrayBytes<FVoxelDistantCell>(MaximumWindowDistantCells * Parts * Parts);
}

bool FVoxelHeightfieldPageBuild::Advance(const FVoxelSurfaceProxyBuilder& InBuilder,
	FVoxelSurfaceTileData& OutData, bool& bOutComplete, FString& OutError, const TAtomic<bool>* InCancel)
{
	bOutComplete = false;
	if (InCancel && InCancel->Load()) { OutError = TEXT("Canceled"); return false; }
	if (!Shards.IsValidIndex(NextShard)) { OutError = TEXT("Heightfield page result was already taken"); return false; }
	if (!bAllocated)
	{
		SurfacePage.Key = {FIntPoint(Geometry.Key.Coordinate.X, Geometry.Key.Coordinate.Y), Geometry.Key.Level};
		SurfacePage.Geometry = Geometry;
		SurfacePage.Step = Geometry.SampleStepCells;
		SurfacePage.Side = int32(Geometry.Key.SideCells() / SurfacePage.Step) + 1;
		const int32 Count = SurfacePage.Side * SurfacePage.Side;
		SurfacePage.GroundZ.Reserve(Count); SurfacePage.WaterZ.Reserve(Count);
		SurfacePage.SurfaceStates.Reserve(Count); SurfacePage.Biome.Reserve(Count); SurfacePage.Flags.Reserve(Count);
		SurfacePage.GroundZ.SetNumUninitialized(Count); SurfacePage.WaterZ.SetNumUninitialized(Count);
		SurfacePage.SurfaceStates.SetNumUninitialized(Count); SurfacePage.Biome.SetNumUninitialized(Count);
		SurfacePage.Flags.SetNumUninitialized(Count);
		SurfacePage.DistantCells.Reserve(MaximumWindowDistantCells * Shards.Num());
		bAllocated = true;
	}
	FVoxelSurfaceTileData Part;
	if (!InBuilder.Build(SurfacePage.Key, Part, OutError, InCancel, nullptr, nullptr, Geometry, &Shards[NextShard])) return false;
	if (Part.DistantCells.Num() > MaximumWindowDistantCells)
	{ OutError = TEXT("Oversized: distant cells exceed admitted window capacity"); return false; }
	const FIntPoint Offset = Part.LocalMinCells / SurfacePage.Step;
	CopyGrid(Part.GroundZ, Part.Side, SurfacePage.GroundZ, SurfacePage.Side, Offset);
	CopyGrid(Part.WaterZ, Part.Side, SurfacePage.WaterZ, SurfacePage.Side, Offset);
	CopyGrid(Part.SurfaceStates, Part.Side, SurfacePage.SurfaceStates, SurfacePage.Side, Offset);
	CopyGrid(Part.Biome, Part.Side, SurfacePage.Biome, SurfacePage.Side, Offset);
	CopyGrid(Part.Flags, Part.Side, SurfacePage.Flags, SurfacePage.Side, Offset);
	SurfacePage.Revision = FMath::Max(SurfacePage.Revision, Part.Revision);
	SurfacePage.DistantCells.Append(Part.DistantCells);
	if (++NextShard == Shards.Num())
	{
		OutData = MoveTemp(SurfacePage);
		bOutComplete = true;
	}
	return true;
}

bool FVoxelHeightfieldPageBuild::Advance(const FVoxelMacroTerrainBuilder& InBuilder,
	FVoxelMacroTileData& OutData, bool& bOutComplete, FString& OutError, const TAtomic<bool>* InCancel)
{
	bOutComplete = false;
	if (InCancel && InCancel->Load()) { OutError = TEXT("Canceled"); return false; }
	if (!Shards.IsValidIndex(NextShard)) { OutError = TEXT("Heightfield page result was already taken"); return false; }
	if (!bAllocated)
	{
		MacroPage.Key = {FIntPoint(Geometry.Key.Coordinate.X, Geometry.Key.Coordinate.Y), Geometry.Key.Level};
		MacroPage.Geometry = Geometry;
		MacroPage.Step = Geometry.SampleStepCells;
		MacroPage.Side = int32(Geometry.Key.SideCells() / MacroPage.Step) + 1;
		const int32 Count = MacroPage.Side * MacroPage.Side;
		MacroPage.Height.Reserve(Count); MacroPage.WaterHeight.Reserve(Count); MacroPage.WaterKind.Reserve(Count);
		MacroPage.SurfaceStates.Reserve(Count); MacroPage.ForestCoverage.Reserve(Count);
		MacroPage.Height.SetNumUninitialized(Count); MacroPage.WaterHeight.SetNumUninitialized(Count);
		MacroPage.WaterKind.SetNumUninitialized(Count); MacroPage.SurfaceStates.SetNumUninitialized(Count);
		MacroPage.ForestCoverage.SetNumUninitialized(Count);
		MacroPage.DistantCells.Reserve(MaximumWindowDistantCells * Shards.Num());
		bAllocated = true;
	}
	FVoxelMacroTileData Part;
	if (!InBuilder.Build(MacroPage.Key, Part, OutError, InCancel, Geometry, &Shards[NextShard])) return false;
	if (Part.DistantCells.Num() > MaximumWindowDistantCells)
	{ OutError = TEXT("Oversized: distant cells exceed admitted window capacity"); return false; }
	const FIntPoint Offset = Part.LocalMinCells / MacroPage.Step;
	CopyGrid(Part.Height, Part.Side, MacroPage.Height, MacroPage.Side, Offset);
	CopyGrid(Part.WaterHeight, Part.Side, MacroPage.WaterHeight, MacroPage.Side, Offset);
	CopyGrid(Part.WaterKind, Part.Side, MacroPage.WaterKind, MacroPage.Side, Offset);
	CopyGrid(Part.SurfaceStates, Part.Side, MacroPage.SurfaceStates, MacroPage.Side, Offset);
	CopyGrid(Part.ForestCoverage, Part.Side, MacroPage.ForestCoverage, MacroPage.Side, Offset);
	MacroPage.Revision = FMath::Max(MacroPage.Revision, Part.Revision);
	MacroPage.DistantCells.Append(Part.DistantCells);
	if (++NextShard == Shards.Num())
	{
		OutData = MoveTemp(MacroPage);
		bOutComplete = true;
	}
	return true;
}

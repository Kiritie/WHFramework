#pragma once

#include "CoreMinimal.h"
#include "Voxel/Rendering/VoxelStreamingGeometry.h"
#include "Voxel/Streaming/VoxelStreamingSource.h"

class FVoxelCoverageIndex;

struct WHFRAMEWORK_API FVoxelTerrainStageSource
{
	FVoxelStreamingSource Source;
	int32 OuterRadiusCells = 0;
	int32 VerticalHalfExtentCells = 0;
	int32 InnerRadiusCells = 0;
};

struct WHFRAMEWORK_API FVoxelTerrainPageCost
{
	uint64 Samples = 0;
	uint64 Triangles = 0;
	uint64 MeshBytes = 0;
	double BuildMilliseconds = 0.0;

	static FVoxelTerrainPageCost Estimate(int64 InSideCells, int32 InStepCells, bool bColumn);
};

struct WHFRAMEWORK_API FVoxelTerrainPageCostLimits
{
	uint64 Triangles = 524288;
	uint64 MeshBytes = 128ull * 1024 * 1024;
	double BuildMilliseconds = 8.0;

	bool Accepts(const FVoxelTerrainPageCost& InCost) const;
};

struct WHFRAMEWORK_API FVoxelTerrainStagePlan
{
	TMap<FVoxelTerrainSpatialKey, FVoxelTerrainGeometry> Pages;
	TMap<FVoxelTerrainSpatialKey, int32> Priorities;
	TSet<FVoxelTerrainSpatialKey> ResolutionFloorPages;
	TMap<FVoxelTerrainSpatialKey, FVoxelTerrainPageCost> Costs;
};

class WHFRAMEWORK_API FVoxelTerrainStagePlanner
{
public:
	static bool Build(EVoxelTerrainRepresentation InRepresentation, TConstArrayView<FVoxelTerrainStageSource> InSources, uint8 InMaximumLevel,
		int32 InMaximumSamplesPerAxis, int32 InWorldMinZ, int32 InWorldMaxZ, double InTargetPixels, uint64 InGeneratorIdentity, uint64 InMaterialRevision,
		FVoxelTerrainStagePlan& OutPlan, FString& OutError, const FVoxelCoverageIndex* InPredecessorCoverage = nullptr,
		const FVoxelTerrainPageCostLimits& InCostLimits = {});

private:
	static bool Visit(const FVoxelTerrainSpatialKey& InKey, TConstArrayView<FVoxelTerrainStageSource> InSources, uint8 InMaximumLevel,
		int32 InMaximumSamplesPerAxis, int32 InWorldMinZ, int32 InWorldMaxZ, double InTargetPixels, uint64 InGeneratorIdentity, uint64 InMaterialRevision,
		FVoxelTerrainStagePlan& InOutPlan, int32& InOutVisited, FString& OutError, const FVoxelCoverageIndex* InPredecessorCoverage,
		const FVoxelTerrainPageCostLimits& InCostLimits);
};

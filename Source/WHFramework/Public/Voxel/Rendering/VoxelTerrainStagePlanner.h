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
	uint8 FirstLevel = 0;
	int32 InnerRadiusCells = 0;
};

struct WHFRAMEWORK_API FVoxelTerrainStagePlan
{
	TMap<FVoxelTerrainSpatialKey, FVoxelTerrainGeometry> Pages;
	TMap<FVoxelTerrainSpatialKey, int32> Priorities;
	TSet<FVoxelTerrainSpatialKey> ResolutionFloorPages;
};

// FirstLevel 由实际上一阶段 frontier 提供；此规划器不读取 Ready、Actor 或任务数量。
class WHFRAMEWORK_API FVoxelTerrainStagePlanner
{
public:
	static bool Build(EVoxelTerrainRepresentation InRepresentation,
		TConstArrayView<FVoxelTerrainStageSource> InSources, uint8 InMaximumLevel,
		int32 InWorldMinZ, int32 InWorldMaxZ, double InTargetPixels,
		uint64 InGeneratorIdentity, uint64 InMaterialRevision,
		FVoxelTerrainStagePlan& OutPlan, FString& OutError,
		const FVoxelCoverageIndex* InPredecessorCoverage = nullptr);

private:
	static bool Visit(const FVoxelTerrainSpatialKey& InKey,
		TConstArrayView<FVoxelTerrainStageSource> InSources, uint8 InMaximumLevel,
		int32 InWorldMinZ, int32 InWorldMaxZ, double InTargetPixels,
		uint64 InGeneratorIdentity, uint64 InMaterialRevision,
		FVoxelTerrainStagePlan& InOutPlan, int32& InOutVisited, FString& OutError,
		const FVoxelCoverageIndex* InPredecessorCoverage);
};

#pragma once

#include "CoreMinimal.h"
#include "Voxel/Rendering/VoxelStreamingGeometry.h"
#include "Voxel/Streaming/VoxelStreamingSource.h"
#include "Voxel/Generation/VoxelGenerationConsumer.h"

class FVoxelCoverageIndex;
class FVoxelTerrainResources;
class FVoxelTerrainResourceLease;
struct FVoxelTerrainStageCacheIndex;
struct FVoxelTerrainStageCacheNode;

// Runtime 候选只复制根引用；变化节点采用 COW，旧候选不能改写已接受的缓存。
struct WHFRAMEWORK_API FVoxelTerrainStageCache
{
	TSharedPtr<const FVoxelTerrainStageCacheIndex, ESPMode::ThreadSafe> Root;
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	FVoxelGenerationConsumerKey Consumer;
	int32 RebuiltNodes = 0;
	int32 ReusedNodes = 0;
	int32 FailedRoots = 0;
	bool bCapacityBlocked = false;
	uint64 RetryCapacityVersion = 0;
	bool IsRetryReady() const;
};

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
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	FVoxelGenerationConsumerKey Consumer;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
	int32 ReservedPages = 0;
	int32 FailedRootCount = 0;
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
		const FVoxelTerrainPageCostLimits& InCostLimits = {}, FVoxelTerrainStageCache* InCache = nullptr);

private:
	static bool Visit(const FVoxelTerrainSpatialKey& InKey, TConstArrayView<FVoxelTerrainStageSource> InSources, uint8 InMaximumLevel,
		int32 InMaximumSamplesPerAxis, int32 InWorldMinZ, int32 InWorldMaxZ, double InTargetPixels, uint64 InGeneratorIdentity, uint64 InMaterialRevision,
		FVoxelTerrainStagePlan& InOutPlan, int32& InOutVisited, FString& OutError, const FVoxelCoverageIndex* InPredecessorCoverage,
		const FVoxelTerrainPageCostLimits& InCostLimits, FVoxelTerrainStageCache* InCache = nullptr,
		TSharedPtr<const FVoxelTerrainStageCacheNode, ESPMode::ThreadSafe> InPrevious = nullptr,
		TSharedPtr<const FVoxelTerrainStageCacheNode, ESPMode::ThreadSafe>* OutCacheNode = nullptr);
};

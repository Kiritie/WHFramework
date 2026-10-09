#pragma once

#include "CoreMinimal.h"
#include "Voxel/Rendering/VoxelStreamingGeometry.h"
#include "Voxel/Streaming/VoxelStreamingSource.h"
#include "Voxel/Generation/VoxelGenerationConsumer.h"
#include "Voxel/Rendering/VoxelCoverageIndex.h"

class FVoxelCoverageIndex;
class FVoxelTerrainResources;
class FVoxelTerrainResourceLease;
struct FVoxelTerrainStageCacheIndex;
struct FVoxelTerrainStageCacheNode;
struct FVoxelTerrainStageWork;
struct FVoxelTerrainDomainSourceBatch;

struct WHFRAMEWORK_API FVoxelTerrainStageSource
{
	FVoxelStreamingSource Source;
	int32 OuterRadiusCells = 0;
	int32 VerticalHalfExtentCells = 0;
	int32 InnerRadiusCells = 0;
};

// Domain 的不可变输入权威；Stage 仅保存同一个 batch 的冻结引用。
struct FVoxelTerrainDomainSourceBatch
{
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	TArray<FVoxelTerrainStageSource> Values;
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

// Runtime 候选只复制根引用；变化节点采用 COW，旧候选不能改写已接受的缓存。
struct WHFRAMEWORK_API FVoxelTerrainStageCache
{
	TSharedPtr<const FVoxelTerrainStageCacheIndex, ESPMode::ThreadSafe> Root;
	TSharedPtr<FVoxelTerrainStageWork, ESPMode::ThreadSafe> Work;
	TSharedPtr<const FVoxelTerrainDomainSourceBatch, ESPMode::ThreadSafe> SourceInputs;
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	FVoxelGenerationConsumerKey Consumer;
	// 仅当前同步调用的旧操作根，不拥有第二个账本。
	const FVoxelTerrainStageCache* CapacityPrevious = nullptr;
	bool bExternalCoverageFloor = false;
	int32 RebuiltNodes = 0;
	int32 ReusedNodes = 0;
	int32 FailedRoots = 0;
	TSharedPtr<const FVoxelTerrainDomainSourceBatch, ESPMode::ThreadSafe> TerminalSources;
	FVoxelCoverageIndex TerminalPredecessor;
	FVoxelTerrainPageCostLimits TerminalLimits;
	uint64 TerminalGenerator = 0, TerminalMaterial = 0;
	double TerminalPixels = 0.0;
	int32 TerminalSamples = 0, TerminalMinZ = 0, TerminalMaxZ = 0;
	uint8 TerminalLevel = 0;
	bool bTerminalAttempt = false;
	bool bCapacityBlocked = false;
	uint64 RetryCapacityVersion = 0;
	bool IsRetryReady() const;
	bool HasPendingWork() const;
	uint64 GetOutputVersion() const;
	bool AccumulateRetainedInput(const FVoxelTerrainResources& InResources, const FVoxelTerrainStageCache* InOther,
		TConstArrayView<uint64> InExcludedSerials, uint64& InOutBytes, int32& InOutLeaseCount, bool bInIncludeCoverage = true) const;
	void VisitRetainedCoverage(TFunctionRef<void(const FVoxelCoverageIndex&)> InVisitor) const;
};

struct WHFRAMEWORK_API FVoxelTerrainStagePlan
{
	FVoxelTerrainStagePlan();
	FVoxelTerrainStagePlan(FVoxelTerrainStagePlan&& InOther);
	FVoxelTerrainStagePlan& operator=(FVoxelTerrainStagePlan&& InOther);
	FVoxelTerrainStagePlan(const FVoxelTerrainStagePlan&) = delete;
	FVoxelTerrainStagePlan& operator=(const FVoxelTerrainStagePlan&) = delete;

	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	FVoxelGenerationConsumerKey Consumer;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
	FVoxelTerrainStageCache* CapacityCache = nullptr;
	int32 ReservedPages = 0;
	int32 FailedRootCount = 0;
	// 原 Build 的已强持输入和 Working；只传聚合值，不建立第二套租约账本。
	uint64 RetainedInputBytes = 0;
	uint64 RetainedWorkingBytes = 0;
	int32 RetainedLeaseCount = 0;
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

	static bool AdvanceDirtyRoots(EVoxelTerrainRepresentation InRepresentation, uint8 InMaximumLevel,
		int32 InMaximumSamplesPerAxis, int32 InWorldMinZ, int32 InWorldMaxZ, double InTargetPixels,
		uint64 InGeneratorIdentity, uint64 InMaterialRevision, FVoxelTerrainStagePlan& OutPlan, FString& OutError,
		const FVoxelCoverageIndex* InPredecessorCoverage, const FVoxelTerrainPageCostLimits& InCostLimits,
		FVoxelTerrainStageCache& InCache, int32 InRootBudget = 64, bool bInExportUnchangedOutput = true);
	static bool ExportCurrentPlan(FVoxelTerrainStageCache& InCache, FVoxelTerrainStagePlan& OutPlan, FString& OutError,
		bool bInInputAlreadyAccounted = false);

private:
	static bool Visit(const FVoxelTerrainSpatialKey& InKey, TConstArrayView<FVoxelTerrainStageSource> InSources, uint8 InMaximumLevel,
		int32 InMaximumSamplesPerAxis, int32 InWorldMinZ, int32 InWorldMaxZ, double InTargetPixels, uint64 InGeneratorIdentity, uint64 InMaterialRevision,
		FVoxelTerrainStagePlan& InOutPlan, int32& InOutVisited, FString& OutError, const FVoxelCoverageIndex* InPredecessorCoverage,
		const FVoxelTerrainPageCostLimits& InCostLimits, FVoxelTerrainStageCache* InCache = nullptr,
		TSharedPtr<const FVoxelTerrainStageCacheNode, ESPMode::ThreadSafe> InPrevious = nullptr,
		TSharedPtr<const FVoxelTerrainStageCacheNode, ESPMode::ThreadSafe>* OutCacheNode = nullptr,
		uint8 InMaximumPageLevel = MAX_uint8);
};

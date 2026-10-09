#pragma once

#include "CoreMinimal.h"
#include "Voxel/Streaming/VoxelInterest.h"
#include "Voxel/Generation/VoxelWorldManifest.h"
#include "Voxel/Rendering/VoxelTerrainStagePlanner.h"
#include "Voxel/Rendering/VoxelCoverageIndex.h"

struct FVoxelTerrainDomainSourceBatch;

class WHFRAMEWORK_API FVoxelTerrainDomainPlanner
{
public:
	static bool Build(TConstArrayView<FVoxelStreamingSource> InSources,
		const FVoxelWorldManifest& InManifest, const FVoxelViewSettings& InSettings,
		FVoxelInterestSet& InOutInterest, FString& OutError, FVoxelTerrainDomainPlanner* InPersistent = nullptr);
	void BindResources(TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
		const FVoxelGenerationConsumerKey& InConsumer);
	bool ApplySourceDelta(EVoxelTerrainRepresentation InRepresentation, TConstArrayView<FVoxelTerrainStageSource> InSources, FString& OutError);
	bool AdvanceDirtyRoots(EVoxelTerrainRepresentation InRepresentation, uint8 InMaximumLevel, int32 InMaximumSamples,
		int32 InMinZ, int32 InMaxZ, double InTargetPixels, uint64 InGenerator, uint64 InMaterial,
		FVoxelTerrainStagePlan& OutPlan, FString& OutError, const FVoxelCoverageIndex* InPredecessor,
		const FVoxelTerrainPageCostLimits& InLimits);
	void Reset();
	bool HasRetryReady() const;
	bool HasPendingWork() const;

private:
	bool AccumulateRetainedInput(const FVoxelTerrainResources& InResources, const FVoxelTerrainDomainPlanner* InOther,
		TConstArrayView<uint64> InExcludedSerials, int32 InSkipStage, uint64& InOutBytes, int32& InOutLeaseCount) const;
	FVoxelCoverageIndex FinePredecessor;
	FVoxelCoverageIndex Predecessors[3];
	uint64 PrefixAppliedVersions[3] = {};
	FVoxelTerrainStageCache Stages[3];
	TSharedPtr<const FVoxelTerrainDomainSourceBatch, ESPMode::ThreadSafe> Sources[3];
	uint64 AdditionalRequiredInput = 0, AdditionalRequiredWorking = 0;
	int32 AdditionalRequiredCount = 0;
	bool bCapacityBlocked = false;
	uint64 RetryCapacityVersion = 0;
};

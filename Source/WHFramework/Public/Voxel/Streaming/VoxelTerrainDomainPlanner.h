#pragma once

#include "CoreMinimal.h"
#include "Voxel/Streaming/VoxelInterest.h"
#include "Voxel/Generation/VoxelWorldManifest.h"
#include "Voxel/Rendering/VoxelTerrainStagePlanner.h"

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

private:
	FVoxelTerrainStageCache Stages[3];
	TSharedPtr<const FVoxelTerrainDomainSourceBatch, ESPMode::ThreadSafe> Sources[3];
};

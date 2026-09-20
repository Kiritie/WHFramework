#pragma once

#include "CoreMinimal.h"
#include "Voxel/Generation/Caves/VoxelCaveGenerator.h"
#include "Voxel/Generation/Hydrology/VoxelHydrology.h"
#include "Voxel/Generation/VoxelFeaturePlan.h"
#include "Voxel/Generation/VoxelStructurePlan.h"

using FVoxelHydrologyPlanPtr = TSharedPtr<const FVoxelHydrologyPlan, ESPMode::ThreadSafe>;
using FVoxelCavePlanPtr = TSharedPtr<const FVoxelCavePlan, ESPMode::ThreadSafe>;
using FVoxelFeaturePlanPtr = TSharedPtr<const FVoxelFeaturePlan, ESPMode::ThreadSafe>;
using FVoxelStructurePlanPtr = TSharedPtr<const FVoxelStructurePlan, ESPMode::ThreadSafe>;

class WHFRAMEWORK_API FVoxelGenerationPlanCache
{
public:
	bool FindHydrology(const FVoxelHydrologyRegionKey& InKey, FVoxelHydrologyPlanPtr& OutPlan) const;
	void StoreHydrology(const FVoxelHydrologyRegionKey& InKey, FVoxelHydrologyPlanPtr InPlan);
	bool FindCave(const FVoxelGenerationTileKey& InKey, FVoxelCavePlanPtr& OutPlan) const;
	void StoreCave(const FVoxelGenerationTileKey& InKey, FVoxelCavePlanPtr InPlan);
	bool FindFeature(const FVoxelGenerationTileKey& InKey, FVoxelFeaturePlanPtr& OutPlan) const;
	void StoreFeature(const FVoxelGenerationTileKey& InKey, FVoxelFeaturePlanPtr InPlan);
	bool FindStructure(const FVoxelGenerationTileKey& InKey, FVoxelStructurePlanPtr& OutPlan) const;
	void StoreStructure(const FVoxelGenerationTileKey& InKey, FVoxelStructurePlanPtr InPlan);
	void Reset();
	uint64 GetAllocatedBytes() const;

private:
	mutable FRWLock Lock;
	TMap<FVoxelHydrologyRegionKey, FVoxelHydrologyPlanPtr> Hydrology;
	TMap<FVoxelGenerationTileKey, FVoxelCavePlanPtr> Caves;
	TMap<FVoxelGenerationTileKey, FVoxelFeaturePlanPtr> Features;
	TMap<FVoxelGenerationTileKey, FVoxelStructurePlanPtr> Structures;
};

#pragma once

#include "CoreMinimal.h"
#include "Voxel/Generation/Caves/VoxelCaveGenerator.h"
#include "Voxel/Generation/Hydrology/VoxelHydrology.h"
#include "Voxel/Generation/VoxelFeaturePlan.h"
#include "Voxel/Generation/VoxelStructurePlan.h"

using FVoxelHydrologyPlanPtr =
	TSharedPtr<
		const FVoxelHydrologyPlan,
		ESPMode::ThreadSafe>;

using FVoxelCavePlanPtr =
	TSharedPtr<
		const FVoxelCavePlan,
		ESPMode::ThreadSafe>;

using FVoxelFeaturePlanPtr =
	TSharedPtr<
		const FVoxelFeaturePlan,
		ESPMode::ThreadSafe>;

using FVoxelStructurePlanPtr =
	TSharedPtr<
		const FVoxelStructurePlan,
		ESPMode::ThreadSafe>;

class WHFRAMEWORK_API FVoxelGenerationPlanCache
{
public:
	bool FindHydrology(
		const FVoxelHydrologyRegionKey& InKey,
		FVoxelHydrologyPlanPtr& OutPlan) const;

	void StoreHydrology(
		const FVoxelHydrologyRegionKey& InKey,
		FVoxelHydrologyPlanPtr InPlan);

	bool GetOrBuildHydrology(
		const FVoxelHydrologyRegionKey& InKey,
		TFunctionRef<bool(
			FVoxelHydrologyPlan&,
			FString&)> InBuild,
		FVoxelHydrologyPlanPtr& OutPlan,
		FString& OutError);

	bool FindCave(
		const FVoxelGenerationTileKey& InKey,
		FVoxelCavePlanPtr& OutPlan) const;

	void StoreCave(
		const FVoxelGenerationTileKey& InKey,
		FVoxelCavePlanPtr InPlan);

	bool GetOrBuildCave(
		const FVoxelGenerationTileKey& InKey,
		TFunctionRef<bool(
			FVoxelCavePlan&,
			FString&)> InBuild,
		FVoxelCavePlanPtr& OutPlan,
		FString& OutError);

	bool FindFeature(
		const FVoxelGenerationTileKey& InKey,
		FVoxelFeaturePlanPtr& OutPlan) const;

	void StoreFeature(
		const FVoxelGenerationTileKey& InKey,
		FVoxelFeaturePlanPtr InPlan);

	bool GetOrBuildFeature(
		const FVoxelGenerationTileKey& InKey,
		TFunctionRef<bool(
			FVoxelFeaturePlan&,
			FString&)> InBuild,
		FVoxelFeaturePlanPtr& OutPlan,
		FString& OutError);

	bool FindStructure(
		const FVoxelGenerationTileKey& InKey,
		FVoxelStructurePlanPtr& OutPlan) const;

	void StoreStructure(
		const FVoxelGenerationTileKey& InKey,
		FVoxelStructurePlanPtr InPlan);

	bool GetOrBuildStructure(
		const FVoxelGenerationTileKey& InKey,
		TFunctionRef<bool(
			FVoxelStructurePlan&,
			FString&)> InBuild,
		FVoxelStructurePlanPtr& OutPlan,
		FString& OutError);

	void Reset();
	uint64 GetAllocatedBytes() const;

private:
	struct FBuildGate;

private:
	mutable FRWLock Lock;

	TMap<
		FVoxelHydrologyRegionKey,
		FVoxelHydrologyPlanPtr> Hydrology;

	TMap<
		FVoxelGenerationTileKey,
		FVoxelCavePlanPtr> Caves;

	TMap<
		FVoxelGenerationTileKey,
		FVoxelFeaturePlanPtr> Features;

	TMap<
		FVoxelGenerationTileKey,
		FVoxelStructurePlanPtr> Structures;

	TMap<
		FVoxelHydrologyRegionKey,
		TSharedPtr<
			FBuildGate,
			ESPMode::ThreadSafe>>
		HydrologyBuilds;

	TMap<
		FVoxelGenerationTileKey,
		TSharedPtr<
			FBuildGate,
			ESPMode::ThreadSafe>>
		CaveBuilds;

	TMap<
		FVoxelGenerationTileKey,
		TSharedPtr<
			FBuildGate,
			ESPMode::ThreadSafe>>
		FeatureBuilds;

	TMap<
		FVoxelGenerationTileKey,
		TSharedPtr<
			FBuildGate,
			ESPMode::ThreadSafe>>
		StructureBuilds;
};

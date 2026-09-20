#pragma once

#include "CoreMinimal.h"
#include "Voxel/Generation/Caves/VoxelCaveGenerator.h"
#include "Voxel/Generation/Hydrology/VoxelHydrology.h"
#include "Voxel/Generation/VoxelFeaturePlan.h"
#include "Voxel/Generation/VoxelNaturalGenerationCache.h"
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

using FVoxelBaseColumnEntryPtr = TSharedPtr<const FVoxelBaseColumnEntry, ESPMode::ThreadSafe>;
using FVoxelRiverFieldTilePtr = TSharedPtr<const FVoxelRiverFieldTile, ESPMode::ThreadSafe>;
using FVoxelLakeAnchorPlanPtr = TSharedPtr<const FVoxelLakeAnchorPlan, ESPMode::ThreadSafe>;
using FVoxelNaturalColumnEntryPtr = TSharedPtr<const FVoxelNaturalColumnEntry, ESPMode::ThreadSafe>;

class WHFRAMEWORK_API FVoxelGenerationPlanCache
{
public:
	bool GetOrBuildBaseColumn(
		const FIntPoint& InPosition,
		TFunctionRef<bool(FVoxelBaseColumnEntry&, FString&)> InBuild,
		FVoxelBaseColumnEntryPtr& OutEntry,
		FString& OutError);

	bool GetOrBuildRiverField(
		const FVoxelNaturalTileKey& InKey,
		TFunctionRef<bool(FVoxelRiverFieldTile&, FString&)> InBuild,
		FVoxelRiverFieldTilePtr& OutTile,
		FString& OutError);

	bool GetOrBuildLake(
		const FVoxelLakeAnchorKey& InKey,
		TFunctionRef<bool(FVoxelLakeAnchorPlan&, FString&)> InBuild,
		FVoxelLakeAnchorPlanPtr& OutPlan,
		FString& OutError);

	bool GetOrBuildNaturalColumn(
		const FIntPoint& InPosition,
		TFunctionRef<bool(FVoxelNaturalColumnEntry&, FString&)> InBuild,
		FVoxelNaturalColumnEntryPtr& OutEntry,
		FString& OutError);

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
	void TrimNaturalCaches(
		TConstArrayView<FIntPoint> InCenters,
		int32 InKeepRadiusCells);
	uint64 GetAllocatedBytes() const;

private:
	struct FBuildGate;

	template<typename KeyType, typename ValueType>
	bool GetOrBuildNatural(
		const KeyType& InKey,
		TMap<KeyType, TSharedPtr<const ValueType, ESPMode::ThreadSafe>>& InValues,
		TMap<KeyType, TSharedPtr<FBuildGate, ESPMode::ThreadSafe>>& InBuilds,
		TFunctionRef<bool(ValueType&, FString&)> InBuild,
		TSharedPtr<const ValueType, ESPMode::ThreadSafe>& OutValue,
		FString& OutError);

private:
	mutable FRWLock Lock;
	mutable FRWLock NaturalLock;

	TMap<FIntPoint, FVoxelBaseColumnEntryPtr> BaseColumns;
	TMap<FVoxelNaturalTileKey, FVoxelRiverFieldTilePtr> RiverFields;
	TMap<FVoxelLakeAnchorKey, FVoxelLakeAnchorPlanPtr> Lakes;
	TMap<FIntPoint, FVoxelNaturalColumnEntryPtr> NaturalColumns;

	TMap<FIntPoint, TSharedPtr<FBuildGate, ESPMode::ThreadSafe>> BaseColumnBuilds;
	TMap<FVoxelNaturalTileKey, TSharedPtr<FBuildGate, ESPMode::ThreadSafe>> RiverFieldBuilds;
	TMap<FVoxelLakeAnchorKey, TSharedPtr<FBuildGate, ESPMode::ThreadSafe>> LakeBuilds;
	TMap<FIntPoint, TSharedPtr<FBuildGate, ESPMode::ThreadSafe>> NaturalColumnBuilds;

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

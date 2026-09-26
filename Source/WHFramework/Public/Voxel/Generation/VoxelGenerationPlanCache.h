#pragma once

#include "CoreMinimal.h"

#include "Voxel/Generation/Caves/VoxelCaveGenerator.h"
#include "Voxel/Generation/Ecology/VoxelEcology.h"
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

using FVoxelEcologyPlanPtr =
	TSharedPtr<
		const FVoxelEcologyPlan,
		ESPMode::ThreadSafe>;

using FVoxelStructurePlanPtr =
	TSharedPtr<
		const FVoxelStructurePlan,
		ESPMode::ThreadSafe>;

using FVoxelBaseColumnEntryPtr =
	TSharedPtr<
		const FVoxelBaseColumnEntry,
		ESPMode::ThreadSafe>;

using FVoxelNaturalColumnEntryPtr =
	TSharedPtr<
		const FVoxelNaturalColumnEntry,
		ESPMode::ThreadSafe>;

struct WHFRAMEWORK_API FVoxelGenerationCacheRetentionPoint
{
	FIntPoint Center = FIntPoint::ZeroValue;
	int32 NaturalRadiusCells = 0;
	int32 PlanRadiusCells = 0;
	int32 HydrologyRadiusCells = 0;
};

struct WHFRAMEWORK_API FVoxelGenerationCacheRetention
{
	TArray<FVoxelGenerationCacheRetentionPoint> Points;
	int32 HydrologyRegionSide = 256;
	int32 HydrologyCellSize = 1;

	uint64 Revision = 0;
};

struct WHFRAMEWORK_API FVoxelGenerationCacheStats
{
	int32 BaseColumns = 0;
	int32 NaturalColumns = 0;
	int32 Hydrology = 0;
	int32 Caves = 0;
	int32 Ecology = 0;
	int32 Features = 0;
	int32 Structures = 0;

	uint64 AllocatedBytes = 0;

	uint64 GateWaitCount = 0;
	uint64 GateWaitMicroseconds = 0;
};

class WHFRAMEWORK_API FVoxelGenerationPlanCache
{
public:
	static constexpr int32 ShardCount = 32;

public:
	explicit FVoxelGenerationPlanCache(
		bool bInAllowGameThreadBuilds = true);

	~FVoxelGenerationPlanCache();

	bool GetOrBuildBaseColumn(
		const FIntPoint& InPosition,
		TFunctionRef<bool(
			FVoxelBaseColumnEntry&,
			FString&)> InBuild,
		FVoxelBaseColumnEntryPtr& OutEntry,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr);

	bool GetOrBuildNaturalColumn(
		const FIntPoint& InPosition,
		TFunctionRef<bool(
			FVoxelNaturalColumnEntry&,
			FString&)> InBuild,
		FVoxelNaturalColumnEntryPtr& OutEntry,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr);

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
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr);

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
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr);

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
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr);

	bool GetOrBuildEcology(
		const FVoxelEcologyTileKey& InKey,
		TFunctionRef<bool(FVoxelEcologyPlan&, FString&)> InBuild,
		FVoxelEcologyPlanPtr& OutPlan,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr);

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
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr);

	void UpdateRetention(
		const FVoxelGenerationCacheRetention& InRetention);

	/**
	 * 必须是有预算的增量维护。
	 * 允许 GameThread 调用，但单帧最多检查 MaxEntries。
	 */
	void TickMaintenance(
		int32 InMaxEntries = 256);

	void Reset();

	FVoxelGenerationCacheStats GetStats() const;
	uint64 GetAllocatedBytes() const;

private:
	struct FBuildGate;

	struct FNaturalShard
	{
		mutable FRWLock Lock;

		TMap<FIntPoint, FVoxelBaseColumnEntryPtr> BaseColumns;
		TMap<FIntPoint, FVoxelNaturalColumnEntryPtr> NaturalColumns;

		TMap<FIntPoint, TSharedPtr<FBuildGate, ESPMode::ThreadSafe>> BaseColumnBuilds;
		TMap<FIntPoint, TSharedPtr<FBuildGate, ESPMode::ThreadSafe>> NaturalColumnBuilds;

		TArray<FIntPoint> BaseColumnKeys;
		TArray<FIntPoint> NaturalColumnKeys;

		int32 BaseColumnCursor = 0;
		int32 NaturalColumnCursor = 0;
	};

	struct FPlanShard
	{
		mutable FRWLock Lock;

		TMap<FVoxelHydrologyRegionKey, FVoxelHydrologyPlanPtr> Hydrology;
		TMap<FVoxelGenerationTileKey, FVoxelCavePlanPtr> Caves;
		TMap<FVoxelEcologyTileKey, FVoxelEcologyPlanPtr> Ecology;
		TMap<FVoxelGenerationTileKey, FVoxelFeaturePlanPtr> Features;
		TMap<FVoxelGenerationTileKey, FVoxelStructurePlanPtr> Structures;

		TMap<FVoxelHydrologyRegionKey, TSharedPtr<FBuildGate, ESPMode::ThreadSafe>> HydrologyBuilds;
		TMap<FVoxelGenerationTileKey, TSharedPtr<FBuildGate, ESPMode::ThreadSafe>> CaveBuilds;
		TMap<FVoxelEcologyTileKey, TSharedPtr<FBuildGate, ESPMode::ThreadSafe>> EcologyBuilds;
		TMap<FVoxelGenerationTileKey, TSharedPtr<FBuildGate, ESPMode::ThreadSafe>> FeatureBuilds;
		TMap<FVoxelGenerationTileKey, TSharedPtr<FBuildGate, ESPMode::ThreadSafe>> StructureBuilds;

		TArray<FVoxelHydrologyRegionKey> HydrologyKeys;
		TArray<FVoxelGenerationTileKey> CaveKeys;
		TArray<FVoxelEcologyTileKey> EcologyKeys;
		TArray<FVoxelGenerationTileKey> FeatureKeys;
		TArray<FVoxelGenerationTileKey> StructureKeys;

		int32 HydrologyCursor = 0;
		int32 CaveCursor = 0;
		int32 EcologyCursor = 0;
		int32 FeatureCursor = 0;
		int32 StructureCursor = 0;
	};

	int32 NaturalShardIndex(const FIntPoint& InKey) const;

	int32 PlanShardIndex(const FVoxelHydrologyRegionKey& InKey) const;
	int32 PlanShardIndex(const FVoxelGenerationTileKey& InKey) const;
	int32 PlanShardIndex(const FVoxelEcologyTileKey& InKey) const;

	bool IsRetained(
		const FIntPoint& InPosition,
		int32 FVoxelGenerationCacheRetentionPoint::* InRadius,
		const FVoxelGenerationCacheRetention& InRetention) const;

	void RecordGateWait(uint64 InMicroseconds);

private:
	TArray<TUniquePtr<FNaturalShard>> NaturalShards;
	TArray<TUniquePtr<FPlanShard>> PlanShards;

	mutable FRWLock RetentionLock;
	FVoxelGenerationCacheRetention Retention;

	int32 MaintenanceShardCursor = 0;

	TAtomic<uint64> GateWaitCount { 0 };
	TAtomic<uint64> GateWaitMicroseconds { 0 };

	bool bAllowGameThreadBuilds = true;
};

#pragma once

#include "CoreMinimal.h"
#include "Voxel/Generation/VoxelGenerationBinding.h"
#include "Voxel/Generation/VoxelGenerationPlanCache.h"

class FVoxelAquiferGenerator;
class FVoxelBiomeGenerator;
class FVoxelCaveGenerator;
class FVoxelClimateGenerator;
class FVoxelEcologyGenerator;
class FVoxelHydrologyGenerator;
using FVoxelHydrologyPlanner = FVoxelHydrologyGenerator;
class FVoxelSurfaceGenerator;
class FVoxelTerrainGenerator;

struct WHFRAMEWORK_API FVoxelGenerationPlanKeys
{
	TArray<FVoxelGenerationTileKey> Tiles;
	TArray<FVoxelEcologyTileKey> EcologyTiles;
};

class WHFRAMEWORK_API FVoxelGenerationQuery
{
public:
	static constexpr int32 GenerationPlanTileSide = 256;
	static constexpr int32 EcologyTileSide = 64;

	static FVoxelGenerationPlanKeys GatherPlanKeys(
		const FVoxelGenerationBounds& InBounds,
		const FVoxelGenerationSettings& InSettings);

	static void GatherStructureTiles(
		const FVoxelGenerationBounds& InBounds,
		TArray<FVoxelGenerationTileKey>& OutTiles);

	static FVoxelHydrologyRegionKey HydrologyKeyForVoxel(
		int32 InX,
		int32 InY,
		const FVoxelGenerationSettings& InSettings);

	bool EnsureHydrologyPlan(
		const FVoxelHydrologyRegionKey& InKey,
		FVoxelHydrologyPlanPtr& OutPlan,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr) const;

	bool EnsureCavePlan(
		const FVoxelGenerationTileKey& InKey,
		FVoxelCavePlanPtr& OutPlan,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr) const;

	bool EnsureStructurePlan(
		const FVoxelGenerationTileKey& InKey,
		FVoxelStructurePlanPtr& OutPlan,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr) const;

	bool EnsureFeaturePlan(
		const FVoxelGenerationTileKey& InKey,
		FVoxelFeaturePlanPtr& OutPlan,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr) const;

	bool EnsureEcologyPlan(
		const FVoxelEcologyTileKey& InKey,
		FVoxelEcologyPlanPtr& OutPlan,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr) const;

	static bool Create(
		TSharedRef<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> InConfig,
		TSharedRef<FVoxelGenerationPlanCache, ESPMode::ThreadSafe> InCache,
		FVoxelGenerationQuery& OutQuery,
		FString& OutError,
		bool bInUseColumnCache = true,
		bool bInRequireReadyHydrology = false,
		bool bInRequireReadyPlans = false);

	bool PrepareColumns(
		const FVoxelGenerationBounds& InBounds,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr);

	bool Prepare(
		const FVoxelGenerationBounds& InBounds,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr);

	bool PrepareStructuresOnly(
		const FVoxelGenerationBounds& InBounds,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr);

	bool SampleEnvironmentColumn(
		int32 InX,
		int32 InY,
		FVoxelColumnSample& OutColumn,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr) const;

	bool ResolveSurfaceCandidate(
		int32 InX,
		int32 InY,
		FVoxelSurfaceCandidate& OutCandidate,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr) const;

	bool SampleColumn(
		int32 InX,
		int32 InY,
		FVoxelColumnSample& OutColumn,
		FString& OutError) const;

	bool SampleSymbol(
		const FIntVector& InPosition,
		uint32& OutValue,
		FString& OutError) const;

	bool SampleColumnSymbols(const FIntPoint& InColumn, int32 InMinZ, int32 InCount,
		TArray<uint32>& OutValues, FString& OutError) const;

	TConstArrayView<FVoxelStructurePlanPtr>
		GetPreparedStructurePlans() const;

private:
	bool SamplePlanColumn(
		const FIntVector& InPosition,
		FVoxelColumnSample& OutColumn,
		const TAtomic<bool>* InCancel) const;

	bool SamplePlanSymbol(
		const FIntVector& InPosition,
		uint32& OutSymbol,
		const TAtomic<bool>* InCancel) const;

	bool ResolveSymbol(const FIntVector& InPosition, FVoxelColumnSample InColumn,
		uint32& OutValue, FString& OutError) const;

	bool GetBaseColumn(
		int32 InX,
		int32 InY,
		FVoxelColumnSample& OutColumn,
		FString& OutError,
		const TAtomic<bool>* InCancel) const;

	bool ComputeBaseColumn(
		int32 InX,
		int32 InY,
		FVoxelColumnSample& OutColumn) const;

	bool ComputeNaturalColumn(
		int32 InX,
		int32 InY,
		FVoxelColumnSample& OutColumn,
		FString& OutError,
		const TAtomic<bool>* InCancel) const;

	bool ApplyHydrology(
		int32 InX,
		int32 InY,
		FVoxelColumnSample& InOutColumn,
		FString& OutError,
		const TAtomic<bool>* InCancel) const;


	bool IsInsidePreparedXY(
		int32 InX,
		int32 InY) const;

	bool ApplyStage(
		EVoxelGenerationStage InStage,
		const FIntVector& InPosition,
		FVoxelColumnSample& InOutColumn,
		uint32& InOutValue,
		FString& OutError) const;

private:
	TSharedPtr<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> Config;
	TSharedPtr<FVoxelGenerationPlanCache, ESPMode::ThreadSafe> Cache;
	TSharedPtr<const FVoxelClimateGenerator, ESPMode::ThreadSafe> Climate;
	TSharedPtr<const FVoxelTerrainGenerator, ESPMode::ThreadSafe> Terrain;
	TSharedPtr<const FVoxelBiomeGenerator, ESPMode::ThreadSafe> Biome;
	TSharedPtr<const FVoxelHydrologyPlanner, ESPMode::ThreadSafe> Hydrology;
	TSharedPtr<const FVoxelCaveGenerator, ESPMode::ThreadSafe> Cave;
	TSharedPtr<const FVoxelAquiferGenerator, ESPMode::ThreadSafe> Aquifer;
	TSharedPtr<const FVoxelSurfaceGenerator, ESPMode::ThreadSafe> Surface;
	TArray<FVoxelCavePlanPtr> PreparedCaves;
	TSharedPtr<const FVoxelEcologyGenerator, ESPMode::ThreadSafe> Ecology;
	TArray<FVoxelEcologyPlanPtr> PreparedEcology;
	TArray<FVoxelFeaturePlanPtr> PreparedFeatures;
	TArray<FVoxelStructurePlanPtr> PreparedStructures;
	FVoxelGenerationBounds PreparedBounds;
	const TAtomic<bool>* Cancel = nullptr;
	bool bUseColumnCache = true;
	bool bRequireReadyHydrology = false;
	bool bRequireReadyPlans = false;
	bool bColumnsPrepared = false;
	bool bSymbolsPrepared = false;
	mutable FString PlanSampleError;
};

#pragma once

#include "CoreMinimal.h"
#include "Voxel/Generation/VoxelGenerationBinding.h"
#include "Voxel/Generation/VoxelGenerationPlanCache.h"

class FVoxelAquiferGenerator;
class FVoxelBiomeGenerator;
class FVoxelCaveGenerator;
class FVoxelClimateGenerator;
class FVoxelEcologyGenerator;
class FVoxelLakeGenerator;
class FVoxelRiverGenerator;
class FVoxelSurfaceGenerator;
class FVoxelTerrainGenerator;

class WHFRAMEWORK_API FVoxelGenerationQuery
{
public:
	static bool Create(
		TSharedRef<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> InConfig,
		TSharedRef<FVoxelGenerationPlanCache, ESPMode::ThreadSafe> InCache,
		FVoxelGenerationQuery& OutQuery,
		FString& OutError);

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

	bool SampleColumn(
		int32 InX,
		int32 InY,
		FVoxelColumnSample& OutColumn,
		FString& OutError) const;

	bool SampleSymbol(
		const FIntVector& InPosition,
		uint32& OutValue,
		FString& OutError) const;

	TConstArrayView<FVoxelStructurePlanPtr>
		GetPreparedStructurePlans() const;

private:
	bool SampleBaseColumn(
		int32 InX,
		int32 InY,
		FVoxelColumnSample& OutColumn) const;

	bool ComputeBaseColumn(
		int32 InX,
		int32 InY,
		FVoxelColumnSample& OutColumn) const;

	bool ComputeNaturalColumn(
		int32 InX,
		int32 InY,
		FVoxelColumnSample& OutColumn,
		FString& OutError) const;

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
	TSharedPtr<const FVoxelRiverGenerator, ESPMode::ThreadSafe> River;
	TSharedPtr<const FVoxelLakeGenerator, ESPMode::ThreadSafe> Lake;
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
	bool bColumnsPrepared = false;
	bool bSymbolsPrepared = false;
};

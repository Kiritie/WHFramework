#pragma once

#include "CoreMinimal.h"
#include "Voxel/Generation/VoxelGenerationBinding.h"
#include "Voxel/Generation/VoxelGenerationPlanCache.h"

class FVoxelClimateGenerator;
class FVoxelTerrainGenerator;
class FVoxelBiomeGenerator;
class FVoxelHydrologyGenerator;
class FVoxelCaveGenerator;
class FVoxelAquiferGenerator;
class FVoxelSurfaceGenerator;

class WHFRAMEWORK_API FVoxelGenerationQuery
{
public:
	static bool Create(TSharedRef<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> InConfig,
		TSharedRef<FVoxelGenerationPlanCache, ESPMode::ThreadSafe> InCache,
		FVoxelGenerationQuery& OutQuery, FString& OutError);

public:
	bool Prepare(const FVoxelGenerationBounds& InBounds, FString& OutError, const TAtomic<bool>* InCancel = nullptr);
	bool SampleColumn(int32 InX, int32 InY, FVoxelColumnSample& OutColumn, FString& OutError) const;
	bool SampleSymbol(const FIntVector& InPosition, uint32& OutValue, FString& OutError) const;
	TConstArrayView<FVoxelStructurePlanPtr> GetPreparedStructurePlans() const;

private:
	bool SampleBaseColumn(int32 InX, int32 InY, FVoxelColumnSample& OutColumn) const;
	bool ApplyStage(EVoxelGenerationStage InStage, const FIntVector& InPosition,
		FVoxelColumnSample& InOutColumn, uint32& InOutValue, FString& OutError) const;

private:
	TSharedPtr<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> Config;
	TSharedPtr<FVoxelGenerationPlanCache, ESPMode::ThreadSafe> Cache;
	TSharedPtr<const FVoxelClimateGenerator, ESPMode::ThreadSafe> Climate;
	TSharedPtr<const FVoxelTerrainGenerator, ESPMode::ThreadSafe> Terrain;
	TSharedPtr<const FVoxelBiomeGenerator, ESPMode::ThreadSafe> Biome;
	TSharedPtr<const FVoxelHydrologyGenerator, ESPMode::ThreadSafe> Hydrology;
	TSharedPtr<const FVoxelCaveGenerator, ESPMode::ThreadSafe> Cave;
	TSharedPtr<const FVoxelAquiferGenerator, ESPMode::ThreadSafe> Aquifer;
	TSharedPtr<const FVoxelSurfaceGenerator, ESPMode::ThreadSafe> Surface;
	TArray<FVoxelHydrologyPlanPtr> PreparedHydrology;
	TArray<FVoxelCavePlanPtr> PreparedCaves;
	TArray<FVoxelFeaturePlanPtr> PreparedFeatures;
	TArray<FVoxelStructurePlanPtr> PreparedStructures;
	FVoxelGenerationBounds PreparedBounds;
	bool bPrepared = false;
};

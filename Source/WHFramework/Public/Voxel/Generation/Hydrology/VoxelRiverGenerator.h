#pragma once

#include "CoreMinimal.h"
#include "Voxel/Generation/VoxelGenerationRecipe.h"
#include "Voxel/Generation/VoxelNaturalGenerationCache.h"

using FVoxelBaseColumnLookup = TFunctionRef<bool(int32, int32, FVoxelColumnSample&, FString&)>;

class FVoxelTerrainGenerator;

class WHFRAMEWORK_API FVoxelRiverGenerator
{
public:
	FVoxelRiverGenerator(
		TSharedRef<const FVoxelGenerationRecipe, ESPMode::ThreadSafe> InRecipe,
		TSharedRef<const FVoxelTerrainGenerator, ESPMode::ThreadSafe> InTerrain);

	bool BuildFieldTile(
		const FVoxelNaturalTileKey& InKey,
		FVoxelBaseColumnLookup InBaseColumn,
		FVoxelRiverFieldTile& OutTile,
		FString& OutError,
		const TAtomic<bool>* InCancel = nullptr) const;

	bool ApplyToColumn(int32 InX, int32 InY, FVoxelColumnSample& InOutColumn) const;
	bool ApplyToColumn(
		int32 InX,
		int32 InY,
		const FVoxelRiverFieldSample& InRiver,
		FVoxelColumnSample& InOutColumn) const;
	int32 SampleRiverDistance(int32 InX, int32 InY) const;
	int32 SampleNormalizedDistanceQ16(int32 InX, int32 InY) const;
	FVector2D SampleRiverDirection(int32 InX, int32 InY) const;
	int32 SampleWaterHeight() const;

private:
	int32 SampleBaseRiverField(int32 InX, int32 InY) const;
	int32 SampleTerrainAwareRiverField(int32 InX, int32 InY) const;
	int32 SampleTerrainDifficultyQ15(int32 InX, int32 InY) const;
	int32 SampleBankNoiseQ15(int32 InX, int32 InY) const;
	int32 SampleClosedRiverDistance(int32 InX, int32 InY) const;
	bool ApplyRiverProfile(int32 InX, int32 InY, int32 InDistance, FVoxelColumnSample& InOutColumn) const;
	bool ApplyRiverShore(int32 InX, int32 InY, int32 InDistance, FVoxelColumnSample& InOutColumn) const;

private:
	static constexpr int32 RiverPeriod = 222;
	static constexpr int32 WarpPeriod = 556;
	static constexpr int32 DetailRiverPeriod = 97;
	static constexpr int32 DepthNoisePeriod = 833;
	static constexpr int32 RiverHeightAboveSea = 4;

	TSharedRef<const FVoxelGenerationRecipe, ESPMode::ThreadSafe> Recipe;
	TSharedRef<const FVoxelTerrainGenerator, ESPMode::ThreadSafe> Terrain;
};

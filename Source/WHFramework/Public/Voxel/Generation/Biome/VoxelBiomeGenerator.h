#pragma once

#include "CoreMinimal.h"
#include "Voxel/Generation/VoxelGenerationRecipe.h"

class WHFRAMEWORK_API FVoxelBiomeGenerator
{
public:
    explicit FVoxelBiomeGenerator(TSharedRef<const FVoxelGenerationRecipe, ESPMode::ThreadSafe> InRecipe);

public:
    uint16 Resolve(
        const FVoxelClimateSample& InClimate,
        const FVoxelMacroTerrainSample& InTerrain) const;

    FName GetBiomeId(uint16 InBiomeIndex) const;

private:
    bool Matches(
        const FVoxelBiomeRuntimeDefinition& InBiome,
        const FVoxelClimateSample& InClimate,
        const FVoxelMacroTerrainSample& InTerrain) const;

private:
    TSharedRef<const FVoxelGenerationRecipe, ESPMode::ThreadSafe> Recipe;
};

#pragma once

#include "CoreMinimal.h"
#include "Voxel/Generation/VoxelGenerationRecipe.h"

class FVoxelClimateGenerator;
class FVoxelLandformGenerator;

class WHFRAMEWORK_API FVoxelTerrainGenerator
{
public:
    FVoxelTerrainGenerator(
        TSharedRef<const FVoxelGenerationRecipe, ESPMode::ThreadSafe> InRecipe,
        TSharedRef<const FVoxelClimateGenerator, ESPMode::ThreadSafe> InClimate);

public:
    FVoxelMacroTerrainSample SampleMacro(int32 InX, int32 InY) const;
    FVoxelLandformSample SampleLandform(int32 InX, int32 InY) const;
    int32 SampleDensityQ16(const FIntVector& InCell, const FVoxelMacroTerrainSample& InMacro) const;

private:
    int32 SampleRawHeight(int32 InX, int32 InY, const FVoxelClimateSample& InClimate,
        const FVoxelLandformSample& InLandform) const;
    int32 SampleSlopePermille(int32 InX, int32 InY) const;

private:
    TSharedRef<const FVoxelGenerationRecipe, ESPMode::ThreadSafe> Recipe;
    TSharedRef<const FVoxelClimateGenerator, ESPMode::ThreadSafe> Climate;
    TSharedRef<const FVoxelLandformGenerator, ESPMode::ThreadSafe> Landform;
};

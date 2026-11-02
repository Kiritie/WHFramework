#pragma once

#include "CoreMinimal.h"
#include "Voxel/Generation/VoxelGenerationRecipe.h"
#include "Voxel/Generation/Landform/VoxelLandform.h"

class FVoxelClimateGenerator;
struct FVoxelTerrainClimateSample;

class WHFRAMEWORK_API FVoxelTerrainGenerator
{
public:
    FVoxelTerrainGenerator(
        TSharedRef<const FVoxelGenerationRecipe, ESPMode::ThreadSafe> InRecipe,
        TSharedRef<const FVoxelClimateGenerator, ESPMode::ThreadSafe> InClimate);

public:
    int32 SampleSurfaceHeight(int32 InX, int32 InY) const;
    FVoxelMacroTerrainSample SampleMacro(int32 InX, int32 InY) const;
    FVoxelMacroTerrainSample SampleMacro(int32 InX, int32 InY, const FVoxelClimateSample& InClimate) const;
    FVoxelLandformSample SampleLandform(int32 InX, int32 InY) const;
    int32 SampleDensityQ16(const FIntVector& InCell, const FVoxelMacroTerrainSample& InMacro) const;

private:
    FVoxelMacroTerrainSample SampleMacroFromTerrainClimate(int32 InX, int32 InY, const FVoxelTerrainClimateSample& InClimate) const;
    int32 SampleRawHeight(int32 InX, int32 InY, const FVoxelTerrainClimateSample& InClimate,
        const FVoxelLandformSample& InLandform) const;
    int32 SampleSlopePermille(int32 InX, int32 InY, int32 InCenterHeight) const;

private:
    TSharedRef<const FVoxelGenerationRecipe, ESPMode::ThreadSafe> Recipe;
    TSharedRef<const FVoxelClimateGenerator, ESPMode::ThreadSafe> Climate;
    FVoxelLandformGenerator Landform;
};

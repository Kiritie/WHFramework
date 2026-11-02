#pragma once

#include "CoreMinimal.h"
#include "Voxel/Generation/VoxelGenerationRecipe.h"

// 地形采样只使用这三个分量；完整气候仍由 FVoxelClimateSample 表示。
struct WHFRAMEWORK_API FVoxelTerrainClimateSample
{
    FVoxelTerrainClimateSample(int32 InContinentalnessQ15, int32 InErosionQ15, int32 InRidgeQ15)
        : ContinentalnessQ15(InContinentalnessQ15)
        , ErosionQ15(InErosionQ15)
        , RidgeQ15(InRidgeQ15)
    {
    }

    explicit FVoxelTerrainClimateSample(const FVoxelClimateSample& InClimate)
        : FVoxelTerrainClimateSample(InClimate.ContinentalnessQ15, InClimate.ErosionQ15, InClimate.RidgeQ15)
    {
    }

    int32 ContinentalnessQ15;
    int32 ErosionQ15;
    int32 RidgeQ15;
};

class WHFRAMEWORK_API FVoxelClimateGenerator
{
public:
    explicit FVoxelClimateGenerator(TSharedRef<const FVoxelGenerationRecipe, ESPMode::ThreadSafe> InRecipe);

public:
    FVoxelClimateSample Sample(int32 InX, int32 InY) const;
    FVoxelTerrainClimateSample SampleTerrainClimate(int32 InX, int32 InY) const;
    int32 SampleRidgeQ15(int32 InX, int32 InY) const;

private:
    TSharedRef<const FVoxelGenerationRecipe, ESPMode::ThreadSafe> Recipe;
};

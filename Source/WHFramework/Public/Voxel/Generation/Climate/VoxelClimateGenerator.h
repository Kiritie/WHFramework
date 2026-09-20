#pragma once

#include "CoreMinimal.h"
#include "Voxel/Generation/VoxelGenerationRecipe.h"

class WHFRAMEWORK_API FVoxelClimateGenerator
{
public:
    explicit FVoxelClimateGenerator(TSharedRef<const FVoxelGenerationRecipe, ESPMode::ThreadSafe> InRecipe);

public:
    FVoxelClimateSample Sample(int32 InX, int32 InY) const;

private:
    TSharedRef<const FVoxelGenerationRecipe, ESPMode::ThreadSafe> Recipe;
};

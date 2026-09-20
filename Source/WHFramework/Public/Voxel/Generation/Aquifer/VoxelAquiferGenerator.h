#pragma once

#include "CoreMinimal.h"
#include "Voxel/Generation/VoxelGenerationRecipe.h"

class WHFRAMEWORK_API FVoxelAquiferGenerator
{
public:
    explicit FVoxelAquiferGenerator(TSharedRef<const FVoxelGenerationRecipe, ESPMode::ThreadSafe> InRecipe);

public:
    FVoxelAquiferSample Sample(
        const FIntVector& InCell,
        const FVoxelColumnSample& InColumn,
        int32 InDensityQ16) const;

private:
    TSharedRef<const FVoxelGenerationRecipe, ESPMode::ThreadSafe> Recipe;
};

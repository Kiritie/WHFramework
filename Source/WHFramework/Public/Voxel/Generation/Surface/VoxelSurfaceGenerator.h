#pragma once

#include "CoreMinimal.h"
#include "Voxel/Generation/VoxelGenerationRecipe.h"

class WHFRAMEWORK_API FVoxelSurfaceGenerator
{
public:
    FVoxelSurfaceGenerator(
        TSharedRef<const FVoxelGenerationRecipe, ESPMode::ThreadSafe> InRecipe);

public:
    void ResolveColumn(FVoxelColumnSample& InOutColumn) const;

    uint32 ResolveSymbol(
        const FIntVector& InCell,
        const FVoxelColumnSample& InColumn,
        int32 InDepthFromSurface) const;

private:
    const FVoxelSurfaceRuntimeRuleSet* GetRuleSet(uint16 InBiomeIndex) const;

    bool Matches(
        const FVoxelSurfaceRuntimeRule& InRule,
        const FVoxelColumnSample& InColumn,
        int32 InDepthFromSurface) const;

private:
    TSharedRef<const FVoxelGenerationRecipe, ESPMode::ThreadSafe> Recipe;
};

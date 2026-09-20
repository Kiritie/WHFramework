#pragma once

#include "CoreMinimal.h"
#include "Voxel/Generation/VoxelGenerationRecipe.h"
#include "Voxel/Generation/VoxelGenerationTypes.h"

struct WHFRAMEWORK_API FVoxelFeaturePlanWrite
{
    FIntVector Position =
        FIntVector::ZeroValue;

    uint32 Value = 0;

    EVoxelGenerationStage Stage =
        EVoxelGenerationStage::None;

    FVoxelStableId OwnerId;
};

struct WHFRAMEWORK_API FVoxelFeaturePlan
{
    FVoxelGenerationBounds Bounds;

    TArray<FVoxelFeaturePlanWrite> Writes;

    bool Sample(
        const FIntVector& InPosition,
        EVoxelGenerationStage InStage,
        uint32& OutValue) const;

    uint64 GetAllocatedBytes() const;
};

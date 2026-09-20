#pragma once

#include "CoreMinimal.h"
#include "Voxel/Generation/VoxelGenerationRecipe.h"
#include "Voxel/Generation/VoxelStructure.h"
#include "Voxel/Generation/VoxelGenerationTypes.h"

struct WHFRAMEWORK_API FVoxelStructurePlanWrite
{
    FIntVector Position =
        FIntVector::ZeroValue;

    uint32 Value = 0;

    EVoxelGenerationStage Stage =
        EVoxelGenerationStage::None;

    FVoxelStableId OwnerId;
};

struct WHFRAMEWORK_API FVoxelStructurePlanClear
{
    FVoxelGenerationBounds Bounds;

    EVoxelGenerationStage Stage =
        EVoxelGenerationStage::None;

    FVoxelStableId OwnerId;
};

struct WHFRAMEWORK_API FVoxelStructurePlan
{
    FVoxelGenerationBounds Bounds;

	TArray<FVoxelStructurePlanClear> Clears;
	TArray<FVoxelStructurePlanWrite> Writes;
	TArray<FVoxelStructureDetailPlacement> Details;

    bool IsCleared(
        const FIntVector& InPosition,
        EVoxelGenerationStage InStage) const;

    bool Sample(
        const FIntVector& InPosition,
        EVoxelGenerationStage InStage,
        uint32& OutValue) const;

    uint64 GetAllocatedBytes() const;
};

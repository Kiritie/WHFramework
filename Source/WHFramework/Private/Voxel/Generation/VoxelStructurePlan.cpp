bool FVoxelStructurePlan::IsCleared(
    const FIntVector& InPosition,
    EVoxelGenerationStage InStage) const
{
    for (const FVoxelStructurePlanClear& Clear :
        Clears)
    {
        if (Clear.Stage ==
                InStage &&
            Clear.Bounds.Contains(
                InPosition))
        {
            return true;
        }
    }

    return false;
}

bool FVoxelStructurePlan::Sample(
    const FIntVector& InPosition,
    EVoxelGenerationStage InStage,
    uint32& OutValue) const
{
    bool bFound = false;
    FVoxelStableId WinningOwner;

    for (const FVoxelStructurePlanWrite& Write :
        Writes)
    {
        if (Write.Position !=
                InPosition ||
            Write.Stage !=
                InStage)
        {
            continue;
        }

        if (!bFound ||
            WinningOwner <
                Write.OwnerId)
        {
            WinningOwner =
                Write.OwnerId;

            OutValue =
                Write.Value;

            bFound = true;
        }
    }

    return bFound;
}

uint64 FVoxelStructurePlan::GetAllocatedBytes() const
{
    return sizeof(FVoxelStructurePlan) +
        Clears.GetAllocatedSize() +
        Writes.GetAllocatedSize() +
        Details.GetAllocatedSize();
}
#include "Voxel/Generation/VoxelStructurePlan.h"

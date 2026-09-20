#include "Voxel/Generation/VoxelFeaturePlan.h"

bool FVoxelFeaturePlan::Sample(
    const FIntVector& InPosition,
    EVoxelGenerationStage InStage,
    uint32& OutValue) const
{
    bool bFound = false;
    FVoxelStableId WinningOwner;

    for (const FVoxelFeaturePlanWrite& Write :
        Writes)
    {
        if (Write.Position !=
                InPosition ||
            Write.Stage !=
                InStage)
        {
            continue;
        }

        /**
         * 冲突规则已经固定：
         * Stable InstanceId 小的先执行；
         * 后执行者覆盖前执行者。
         *
         * 因此同 Stage 下更大的 OwnerId 最终获胜。
         */
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

uint64 FVoxelFeaturePlan::GetAllocatedBytes() const
{
    return static_cast<uint64>(
        Writes.GetAllocatedSize());
}

#pragma once

#include "CoreMinimal.h"
#include "Templates/Atomic.h"
#include "Templates/SharedPointer.h"
#include "Voxel/Runtime/VoxelBlockState.h"
#include "Voxel/Save/VoxelDeltaTypes.h"

enum class EVoxelSectionStatus : uint8
{
    None,
    Allocated,
    BaseReady,
    DataReady,
    Failed,
    Unloading
};

struct WHFRAMEWORK_API FVoxelSectionStamp
{
    uint64 Epoch = 0;
    uint64 Token = 0;
    uint64 RecipeHash = 0;

    bool operator==(
        const FVoxelSectionStamp& InOther) const
    {
        return Epoch ==
                InOther.Epoch &&
            Token ==
                InOther.Token &&
            RecipeHash ==
                InOther.RecipeHash;
    }
};

struct WHFRAMEWORK_API FVoxelSection
{
    FVoxelSectionStamp Stamp;

    EVoxelSectionStatus Status =
        EVoxelSectionStatus::None;

    /**
     * Immutable natural base。
     * 可被 Proxy/Compare/Restore Natural 复用。
     */
    TSharedPtr<
        const TArray<FVoxelBlockState>,
        ESPMode::ThreadSafe> BaseBlocks;

    /**
     * DataReady 后的最终 Exact Data。
     * 第一版保持完整 16^3 数组，便于兼容现有编辑/交互代码。
     */
    TArray<FVoxelBlockState> Blocks;

    /**
     * 只存相对 Natural Base 的差异。
     */
    TMap<int32, FVoxelBlockState> Overlay;

	TMap<int32, FVoxelBlockEntityState> Entities;

    uint64 CommittedRevision = 0;
	uint64 PersistedRevision = 0;

    TAtomic<int32> PinCount { 0 };

    uint64 LastWantedFrame = 0;

    bool bCollisionDirty = true;
    bool bFineMeshDirty = true;
    bool bSimulationWanted = false;
};

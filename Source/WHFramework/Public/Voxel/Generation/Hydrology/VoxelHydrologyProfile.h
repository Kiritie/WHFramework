#pragma once
#include "CoreMinimal.h"
#include "Containers/ArrayView.h"


struct WHFRAMEWORK_API FVoxelRiverProfileNode
{
    int32 LowerWaterZ = -32768;
    int32 UpperWaterZ = 32766;
    bool bFixed = false;
    int32 FixedWaterZ = 0;
};

struct WHFRAMEWORK_API FVoxelRiverProfileEdge
{
    int32 Upstream = INDEX_NONE;
    int32 Downstream = INDEX_NONE;
};

namespace VoxelHydrology
{

    WHFRAMEWORK_API bool SolveRiverProfile(
        TConstArrayView<FVoxelRiverProfileNode> InNodes,
        TConstArrayView<FVoxelRiverProfileEdge> InEdges,
        TArray<int32>& OutWaterZ,
        int32& OutConflictNode,
        FString& OutError,
        const TAtomic<bool>* InCancel = nullptr,
        int32* OutConflictLowerWaterZ = nullptr);
}

#pragma once
#include "CoreMinimal.h"
#include "Voxel/Tasks/VoxelTaskTypes.h"
#include "Voxel/Runtime/VoxelBlockState.h"
struct WHFRAMEWORK_API FVoxelSectionSnapshot
{
    FVoxelTaskStamp Stamp;
    TArray<uint32> Blocks;
    TArray<uint32> Halo[6];
    bool Known[6]={false,false,false,false,false,false};
    bool TrySample(const FIntVector& Local,FVoxelBlockState& Out)const;
    uint64 Bytes()const;
};

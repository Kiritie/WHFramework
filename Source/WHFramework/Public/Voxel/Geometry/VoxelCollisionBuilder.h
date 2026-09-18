#pragma once
#include "CoreMinimal.h"
#include <atomic>
#include "Voxel/Chunks/VoxelSectionSnapshot.h"
#include "Voxel/Runtime/VoxelRegistry.h"
#include "Voxel/Geometry/VoxelShapeRegistry.h"
struct WHFRAMEWORK_API FVoxelSectionCollisionResult
{
    FVoxelTaskStamp Stamp;TArray<FBox> Boxes;
};
class WHFRAMEWORK_API FVoxelCollisionBuilder
{
public:
    static bool Build(const FVoxelSectionSnapshot& Snapshot,const FVoxelRegistrySnapshot& Registry,
        const FVoxelShapeRegistry& Shapes,FVoxelSectionCollisionResult& Out,const std::atomic_bool* Cancel=nullptr);
};

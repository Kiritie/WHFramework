#pragma once
#include "CoreMinimal.h"
#include "Voxel/Chunks/VoxelSectionSnapshot.h"
#include "Voxel/Geometry/VoxelShapeRegistry.h"
#include "Voxel/Runtime/VoxelRegistry.h"
#include <atomic>
struct WHFRAMEWORK_API FVoxelSectionCollisionResult
{
	FVoxelTaskStamp Stamp;
	TArray<FBox> Boxes;
};
class WHFRAMEWORK_API FVoxelCollisionBuilder
{
public:
	static bool Build(const FVoxelSectionSnapshot& Snapshot,
	                  const FVoxelRegistrySnapshot& Registry,
	                  const FVoxelShapeRegistry& Shapes,
	                  FVoxelSectionCollisionResult& Out,
	                  const std::atomic_bool* Cancel = nullptr);
};

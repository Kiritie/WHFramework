#pragma once

#include "CoreMinimal.h"
#include "Voxel/Chunks/VoxelSectionSnapshot.h"
#include "Voxel/Task/VoxelTaskScheduler.h"
#include "Voxel/Runtime/VoxelRegistry.h"
#include "Voxel/Geometry/VoxelShapeRegistry.h"

struct WHFRAMEWORK_API FVoxelSectionCollisionResult
{
	FVoxelTaskStamp Stamp;
	TArray<FBox> Boxes;

	uint64 Bytes() const
	{
		return uint64(Boxes.Num()) * sizeof(FBox);
	}
};

class WHFRAMEWORK_API FVoxelCollisionBuilder
{
public:
	static bool Build(
		const FVoxelSectionSnapshot& InSnapshot,
		const FVoxelRegistrySnapshot& InRegistry,
		const FVoxelShapeRegistry& InShapes,
		FVoxelSectionCollisionResult& OutResult,
		const TAtomic<bool>* InCancel = nullptr);
};

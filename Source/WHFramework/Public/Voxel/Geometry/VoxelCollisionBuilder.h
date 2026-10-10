#pragma once

#include "CoreMinimal.h"
#include "Voxel/Chunks/VoxelSectionSnapshot.h"
#include "Voxel/Task/VoxelTaskScheduler.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/Runtime/VoxelRegistry.h"
#include "Voxel/Geometry/VoxelShapeRegistry.h"

struct WHFRAMEWORK_API FVoxelSectionCollisionResult
{
	TSharedPtr<FVoxelTerrainWorkReceipt, ESPMode::ThreadSafe> WorkReceipt;
	FVoxelTaskStamp Stamp;
	TArray<FBox> Boxes;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;

	uint64 Bytes() const
	{
		return VoxelBoundedAllocation::WorkerSharedBytes<FVoxelSectionCollisionResult>() + Boxes.GetAllocatedSize();
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

private:
	static bool VisitBoxes(
		const FVoxelSectionSnapshot& InSnapshot,
		const FVoxelRegistrySnapshot& InRegistry,
		const FVoxelShapeRegistry& InShapes,
		TFunctionRef<bool(const FBox&)> InVisit,
		const TAtomic<bool>* InCancel);
};

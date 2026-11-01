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
	TArray<FVector3f> Vertices;
	TArray<int32> Triangles;
	FBox LocalBounds = FBox(ForceInit);
	uint8 UnknownHaloMask = 0;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;

	uint64 Bytes() const
	{
		return VoxelBoundedAllocation::WorkerSharedBytes<FVoxelSectionCollisionResult>() +
			Vertices.GetAllocatedSize() + Triangles.GetAllocatedSize();
	}
	bool IsEmpty() const { return Triangles.IsEmpty(); }
};

class WHFRAMEWORK_API FVoxelCollisionBuilder
{
public:
	static constexpr int32 MaximumQuads = 262144;
	static uint64 MaximumOutputBytes();
	static bool Build(
		const FVoxelSectionSnapshot& InSnapshot,
		const FVoxelRegistrySnapshot& InRegistry,
		const FVoxelShapeRegistry& InShapes,
		FVoxelSectionCollisionResult& OutResult,
		const TAtomic<bool>* InCancel = nullptr);

private:
	static bool VisitQuads(
		const FVoxelSectionSnapshot& InSnapshot,
		const FVoxelRegistrySnapshot& InRegistry,
		const FVoxelShapeRegistry& InShapes,
		TFunctionRef<bool(const FVector*)> InVisit,
		const TAtomic<bool>* InCancel);
};

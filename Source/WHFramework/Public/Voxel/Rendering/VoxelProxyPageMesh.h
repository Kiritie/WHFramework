#pragma once

#include "CoreMinimal.h"
#include "Voxel/Geometry/VoxelSectionMesher.h"
#include "Voxel/Rendering/VoxelProxyBuilder.h"

class WHFRAMEWORK_API FVoxelProxyPageMesh
{
public:
	bool Initialize(const FVoxelTerrainGeometry& InGeometry, FString& OutError,
		uint64 InRetainedBytes = 12ull * 1024ull * 1024ull, bool bInCountOnly = false);
	uint64 GetEstimatedMeshBytes() const
	{
		return EstimatedMeshBytes;
	}
	bool Advance(const FVoxelVoxelProxyData& InData, const FVoxelRegistrySnapshot& InRegistry,
		const FVoxelShapeRegistry& InShapes, const FVoxelBoundaryTransitionContext* InContext,
		double InTextureRepeats, FVoxelSectionMeshResult& OutMesh, bool& bOutComplete,
		FString& OutError, const TAtomic<bool>* InCancel);

private:
	FVoxelTerrainGeometry Geometry;
	TArray<FVoxelTerrainBuildShard> Shards;
	FVoxelSectionMeshResult Mesh;
	int32 NextShard = 0;
	uint64 RetainedBytes = 0;
	uint64 EstimatedMeshBytes = 0;
	bool bCountOnly = false;
};

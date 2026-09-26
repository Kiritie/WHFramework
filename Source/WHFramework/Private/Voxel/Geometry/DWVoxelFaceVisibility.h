#pragma once

#include "CoreMinimal.h"
#include "Voxel/Runtime/VoxelBlockState.h"

class FVoxelShapeRegistry;
struct FVoxelRegistrySnapshot;
struct FVoxelShapeQuad;

namespace VoxelFaceVisibility
{
	bool ShouldRender(
		const FVoxelRegistrySnapshot& InRegistry,
		const FVoxelShapeRegistry& InShapes,
		FVoxelBlockState InCurrent,
		FVoxelBlockState InNeighbor,
		uint8 InFace,
		const FVoxelShapeQuad* InQuad = nullptr);
}

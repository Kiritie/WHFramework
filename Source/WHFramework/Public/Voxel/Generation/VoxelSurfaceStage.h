#pragma once
#include "CoreMinimal.h"
#include "Voxel/Generation/VoxelGenerationContext.h"
class WHFRAMEWORK_API FVoxelSurfaceStage
{
public:
	static FVoxelBlockState Resolve(const FVoxelGenerationRuntimeConfig& C, const FVoxelColumnSample& Column, int32 Z, int64 Density);
};

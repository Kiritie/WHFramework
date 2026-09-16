#pragma once
#include "CoreMinimal.h"
#include "Voxel/Generation/VoxelGenerationContext.h"
#include "Voxel/Generation/VoxelNoiseRouter.h"
class WHFRAMEWORK_API FVoxelBiomeResolver
{
public:
	static EVoxelBiomeId Resolve(const FVoxelGenerationSettings& S, const FVoxelNoiseRouter& N, int32 X, int32 Y, int32 SurfaceZ);
};

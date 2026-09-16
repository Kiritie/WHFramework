#pragma once
#include "CoreMinimal.h"
#include "Voxel/Generation/VoxelGenerationContext.h"
#include "Voxel/Generation/VoxelNoiseRouter.h"
class WHFRAMEWORK_API FVoxelTerrainStage
{
public:
	explicit FVoxelTerrainStage(const FVoxelGenerationSettings& In)
	    : S(In)
	    , Noise(In.Seed)
	{
	}
	int64 HeightQ16(int32 X, int32 Y) const;
	int64 DensityQ16(int32 X, int32 Y, int32 Z, int64 Height) const;
	FVoxelColumnSample SampleColumn(int32 X, int32 Y) const;

private:
	FVoxelGenerationSettings S;
	FVoxelNoiseRouter Noise;
};

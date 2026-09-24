#pragma once

#include "CoreMinimal.h"
#include "Voxel/Generation/VoxelGenerationRecipe.h"

class WHFRAMEWORK_API FVoxelLandformGenerator
{
public:
	FVoxelLandformGenerator(TSharedRef<const FVoxelGenerationRecipe, ESPMode::ThreadSafe> InRecipe);

	FVoxelLandformSample Sample(int32 InX, int32 InY, const FVoxelClimateSample& InClimate) const;

private:
	TSharedRef<const FVoxelGenerationRecipe, ESPMode::ThreadSafe> Recipe;
};

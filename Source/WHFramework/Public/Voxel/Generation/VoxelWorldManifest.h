#pragma once

#include "CoreMinimal.h"
#include "Voxel/Generation/VoxelGenerationRecipe.h"

struct WHFRAMEWORK_API FVoxelWorldManifest
{
	static constexpr uint32 ProtocolVersion = 4;

	FGuid WorldId;
	uint32 GeneratorVersion = FVoxelGenerationRecipe::CurrentAlgorithmVersion;
	FVoxelGenerationSettings Settings;
	int32 BlockSizeCentimeters = 25;
	uint64 RegistryHash = 0;
	uint64 RecipeHash = 0;
	uint64 BaseSampleHash = 0;
};

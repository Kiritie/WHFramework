#pragma once

#include "CoreMinimal.h"
#include "Voxel/Generation/VoxelGenerationRecipe.h"

struct WHFRAMEWORK_API FVoxelWorldManifest
{
	static constexpr uint32 ProtocolVersion = 4;

	FGuid WorldId;
	uint32 GeneratorVersion =
		FVoxelGenerationRecipe::
			CurrentAlgorithmVersion;

	FVoxelGenerationSettings Settings;

	int32 BlockSizeCentimeters = 25;

	uint64 RegistryHash = 0;
	uint64 RecipeHash = 0;

	// Protocol 5 起保存的是纯 Generation Identity Signature。
	// 字段暂保留原名，避免本轮同时扩大 Network/Save 修改面。
	uint64 BaseSampleHash = 0;
};

#pragma once
#include "Containers/ArrayView.h"
#include "CoreMinimal.h"
#include "Voxel/Generation/VoxelGenerationContext.h"
class WHFRAMEWORK_API FVoxelManifestCodec
{
public:
	static bool Encode(const FVoxelWorldManifest& In, TArray<uint8>& Out);
	static bool Decode(TConstArrayView<uint8> Bytes, FVoxelWorldManifest& Out);
	static uint64 RecipeFingerprint(const FVoxelWorldManifest& In);
};

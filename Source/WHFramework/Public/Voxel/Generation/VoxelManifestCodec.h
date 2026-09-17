#pragma once
#include "CoreMinimal.h"
#include "Containers/ArrayView.h"
#include "Voxel/Generation/VoxelGenerationContext.h"
class WHFRAMEWORK_API FVoxelManifestCodec
{
public:
    static constexpr int32 MaxBytes=1024;
    static uint64 RecipeFingerprint(const FVoxelWorldManifest& Manifest);
    static bool Encode(const FVoxelWorldManifest& Manifest,TArray<uint8>& Out);
    static bool Decode(TConstArrayView<uint8> Bytes,FVoxelWorldManifest& Out);
};

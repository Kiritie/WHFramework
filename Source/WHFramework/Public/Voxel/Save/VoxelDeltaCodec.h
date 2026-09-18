#pragma once
#include "Containers/ArrayView.h"
#include "CoreMinimal.h"
#include "Voxel/Save/VoxelDeltaTypes.h"
#include "Voxel/Runtime/VoxelRegistry.h"
class WHFRAMEWORK_API FVoxelDeltaCodec
{
public:
    static constexpr uint64 MaxSectionWireBytes=512*1024;
    static constexpr uint64 MaxAtomicBatchWireBytes=1536*1024;
    // Conservative bound including stable-name palette and the Zlib envelope; 0 means invalid.
    static uint64 MaxEncodedBytes(const FVoxelRegistrySnapshot& Registry,const FVoxelSectionOverlay& Overlay);
    static bool Encode(const FVoxelWorldManifest& World,const FVoxelRegistrySnapshot& Registry,const FVoxelSectionOverlay& Overlay,TArray<uint8>& Out);
    static bool Decode(TConstArrayView<uint8> Bytes,const FVoxelWorldManifest& World,const FVoxelRegistrySnapshot& Registry,FVoxelSectionOverlay& Out);
};

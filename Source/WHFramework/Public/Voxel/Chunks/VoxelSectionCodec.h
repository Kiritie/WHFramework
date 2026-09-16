#pragma once
#include "Containers/ArrayView.h"
#include "CoreMinimal.h"
#include "Voxel/Chunks/VoxelSectionStorage.h"
class WHFRAMEWORK_API FVoxelSectionCodec
{
public:
	static bool Encode(const FVoxelSectionStorage& Storage, TArray<uint8>& Out);
	static bool Decode(TConstArrayView<uint8> Bytes, TFunctionRef<bool(FVoxelBlockState)> IsValidState, FVoxelSectionStorage& Out);
	static uint64 Fingerprint(const FVoxelSectionStorage& Storage);
};

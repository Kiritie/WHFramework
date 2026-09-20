#pragma once

#include "Containers/ArrayView.h"
#include "CoreMinimal.h"
#include "Voxel/Runtime/VoxelRegistry.h"
#include "Voxel/Save/VoxelDeltaTypes.h"
#include "Voxel/Generation/VoxelWorldManifest.h"

class WHFRAMEWORK_API FVoxelDeltaCodec
{
public:
	static constexpr uint64 MaxSectionWireBytes = 512 * 1024;
	static constexpr uint64 MaxAtomicBatchWireBytes = 1536 * 1024;

	static uint64 MaxEncodedBytes(
		const FVoxelRegistrySnapshot& InRegistry,
		const FVoxelPersistentSection& InSection);
	static bool Encode(
		const FVoxelWorldManifest& InWorld,
		const FVoxelRegistrySnapshot& InRegistry,
		const FVoxelPersistentSection& InSection,
		TArray<uint8>& OutBytes);
	static bool Decode(
		TConstArrayView<uint8> InBytes,
		const FVoxelWorldManifest& InWorld,
		const FVoxelRegistrySnapshot& InRegistry,
		FVoxelPersistentSection& OutSection);
};

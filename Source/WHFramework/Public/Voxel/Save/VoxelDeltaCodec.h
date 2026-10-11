#pragma once

#include "Containers/ArrayView.h"
#include "CoreMinimal.h"
#include "Voxel/Runtime/VoxelRegistry.h"
#include "Voxel/Save/VoxelDeltaTypes.h"
#include "Voxel/Generation/VoxelWorldManifest.h"
#include "Voxel/Task/VoxelTerrainResources.h"
#include "Voxel/Serialization/VoxelBinaryCodec.h"

// 编码借用原固定记录，不为了序列化复制整份 Overlay/实体 Payload。
struct WHFRAMEWORK_API FVoxelDeltaEncodeView
{
	FIntVector Section;
	uint64 Revision;
	const TMap<int32, FVoxelBlockState>& Blocks;
	const TMap<int32, FVoxelBlockEntityState>& Entities;
};

// Worker 先解压到已授予的工作区并计数，再授予实际输出；容量等待保留同一份 Raw。
class WHFRAMEWORK_API FVoxelDeltaDecoder
{
public:
	bool Advance(TConstArrayView<uint8> InBytes, const FVoxelWorldManifest& InWorld,
		const FVoxelRegistrySnapshot& InRegistry,
		TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
		const FVoxelGenerationConsumerKey& InConsumer, EVoxelTerrainResourcePool InOutputPool,
		TSharedPtr<FVoxelPersistentSection, ESPMode::ThreadSafe>& OutSection, FString& OutError);
	uint64 GetRetainedWorkingBytes() const;
	const TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& GetResourceLease() const { return WorkingLease; }

private:
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> WorkingLease;
	TArray<uint8> Raw;
	uint64 OutputBytes = 0;
	uint64 DecodeWorkingBytes = 0;
};

class WHFRAMEWORK_API FVoxelDeltaCodec
{
public:
	static constexpr uint64 MaxSectionWireBytes = (16 * 1024 * 1024) + (64 * 1024);
	static constexpr uint64 MaxAtomicBatchWireBytes = (48 * 1024 * 1024) + (192 * 1024);
	static constexpr int32 MaximumRawBytes = 16 * 1024 * 1024;
	static constexpr int32 MaximumEntityCount = 256;
	static constexpr int32 MaximumEntityPayloadBytes = (1024 * 1024) + 16;

	static uint64 MaxEncodedBytes(
		const FVoxelRegistrySnapshot& InRegistry,
		const FVoxelPersistentSection& InSection);
	static uint64 MaxEncodedBytes(
		const FVoxelRegistrySnapshot& InRegistry,
		const FVoxelDeltaEncodeView& InSection);
	static bool Encode(
		const FVoxelWorldManifest& InWorld,
		const FVoxelRegistrySnapshot& InRegistry,
		const FVoxelPersistentSection& InSection,
		TArray<uint8>& OutBytes);
	static bool Encode(
		const FVoxelWorldManifest& InWorld,
		const FVoxelRegistrySnapshot& InRegistry,
		const FVoxelDeltaEncodeView& InSection,
		TArray<uint8>& OutBytes);
	static bool EncodeGranted(const FVoxelWorldManifest& InWorld, const FVoxelRegistrySnapshot& InRegistry,
		const FVoxelDeltaEncodeView& InSection,
		const TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe>& InResources,
		const FVoxelGenerationConsumerKey& InConsumer, FVoxelBinaryBuffer& OutBuffer, FString& OutError);
	static bool Decode(
		TConstArrayView<uint8> InBytes,
		const FVoxelWorldManifest& InWorld,
		const FVoxelRegistrySnapshot& InRegistry,
		FVoxelPersistentSection& OutSection);
};

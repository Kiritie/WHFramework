#pragma once
#include "Containers/ArrayView.h"
#include "CoreMinimal.h"
#include "Voxel/Network/VoxelNetworkTypes.h"
#include "Voxel/Save/VoxelDeltaCodec.h"
#include "Voxel/Serialization/VoxelBinaryCodec.h"
class WHFRAMEWORK_API FVoxelNetworkCodec
{
public:
	static bool Encode(EVoxelMessage Kind, const FGuid& Session, TConstArrayView<uint8> Payload, TArray<uint8>& Out);
	static bool Decode(TConstArrayView<uint8> Bytes, FVoxelWireMessage& Out);
	static void WriteKey(FVoxelByteWriter& Writer, const FVoxelSectionKey& Key);
	static FVoxelSectionKey ReadKey(FVoxelByteReader& Reader);
	static bool EncodeIntent(const FVoxelEditIntent& Intent, TArray<uint8>& Out);
	static bool DecodeIntent(TConstArrayView<uint8> Bytes, FVoxelEditIntent& Out);
	static bool EncodeSnapshots(const FVoxelSnapshotBatch& Batch,
	                            const FVoxelWorldManifest& Manifest,
	                            const FVoxelRegistrySnapshot& Registry,
	                            TArray<uint8>& Out);
	static bool DecodeSnapshots(TConstArrayView<uint8> Bytes,
	                            const FVoxelWorldManifest& Manifest,
	                            const FVoxelRegistrySnapshot& Registry,
	                            FVoxelSnapshotBatch& Out);
	static bool EncodeReply(const FVoxelEditReply& Reply, TArray<uint8>& Out);
	static bool DecodeReply(TConstArrayView<uint8> Bytes, FVoxelEditReply& Out);
};

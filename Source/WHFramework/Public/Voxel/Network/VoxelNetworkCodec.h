#pragma once

#include "Containers/ArrayView.h"
#include "CoreMinimal.h"
#include "Voxel/Network/VoxelNetworkTypes.h"
#include "Voxel/Serialization/VoxelBinaryCodec.h"

class WHFRAMEWORK_API FVoxelNetworkCodec
{
public:
	static constexpr int32 MaxWireBytes = 2 * 1024 * 1024;
	static constexpr int32 MaxPatchSections = 32;
	static constexpr int32 MaxPatchWrites = 8192;
	static constexpr int32 MaxRepresentationBytes = 1024 * 1024;

	static bool Encode(
		EVoxelMessage InKind,
		const FGuid& InSession,
		TConstArrayView<uint8> InPayload,
		TArray<uint8>& OutBytes);
	static bool Decode(TConstArrayView<uint8> InBytes, FVoxelWireMessage& OutMessage);

	static void WriteVector(FVoxelByteWriter& InWriter, const FIntVector& InValue);
	static FIntVector ReadVector(FVoxelByteReader& InReader);

	static bool EncodeHello(const FVoxelNetworkHello& InValue, TArray<uint8>& OutBytes);
	static bool DecodeHello(TConstArrayView<uint8> InBytes, FVoxelNetworkHello& OutValue);
	static bool EncodeWelcome(const FVoxelNetworkWelcome& InValue, TArray<uint8>& OutBytes);
	static bool DecodeWelcome(TConstArrayView<uint8> InBytes, FVoxelNetworkWelcome& OutValue);
	static bool EncodeReady(const FVoxelNetworkReady& InValue, TArray<uint8>& OutBytes);
	static bool DecodeReady(TConstArrayView<uint8> InBytes, FVoxelNetworkReady& OutValue);
	static bool EncodeChangeSummary(const FVoxelRegionChangeSummary& InValue, TArray<uint8>& OutBytes, FString& OutError);
	static bool DecodeChangeSummary(TConstArrayView<uint8> InBytes, FVoxelRegionChangeSummary& OutValue, FString& OutError);
	static bool EncodeSectionState(const FVoxelSectionState& InValue, TArray<uint8>& OutBytes);
	static bool DecodeSectionState(TConstArrayView<uint8> InBytes, FVoxelSectionState& OutValue);
	static bool EncodeSectionSnapshot(const FVoxelNetworkSectionSnapshot& InValue, TArray<uint8>& OutBytes, FString& OutError);
	static bool DecodeSectionSnapshot(TConstArrayView<uint8> InBytes, FVoxelNetworkSectionSnapshot& OutValue, FString& OutError);
	static bool EncodePatchBatch(const FVoxelNetworkPatchBatch& InValue, TArray<uint8>& OutBytes, FString& OutError);
	static bool DecodePatchBatch(TConstArrayView<uint8> InBytes, FVoxelNetworkPatchBatch& OutValue, FString& OutError);
	static bool EncodeRepresentationRequest(const FVoxelRepresentationRequest& InValue, TArray<uint8>& OutBytes);
	static bool DecodeRepresentationRequest(TConstArrayView<uint8> InBytes, FVoxelRepresentationRequest& OutValue);
	static bool EncodeRepresentationReply(const FVoxelRepresentationReply& InValue, TArray<uint8>& OutBytes, FString& OutError);
	static bool DecodeRepresentationReply(TConstArrayView<uint8> InBytes, FVoxelRepresentationReply& OutValue, FString& OutError);
	static bool EncodeRepresentationInvalidate(const FVoxelRepresentationInvalidate& InValue, TArray<uint8>& OutBytes, FString& OutError);
	static bool DecodeRepresentationInvalidate(TConstArrayView<uint8> InBytes, FVoxelRepresentationInvalidate& OutValue, FString& OutError);
	static bool EncodeIntent(const FVoxelEditIntent& InIntent, TArray<uint8>& OutBytes);
	static bool DecodeIntent(TConstArrayView<uint8> InBytes, FVoxelEditIntent& OutIntent);
	static bool EncodeReply(const FVoxelEditReply& InReply, TArray<uint8>& OutBytes);
	static bool DecodeReply(TConstArrayView<uint8> InBytes, FVoxelEditReply& OutReply);

private:
	static void WriteCellEdit(FVoxelByteWriter& InWriter, const FVoxelSectionCellEdit& InEdit);
	static bool ReadCellEdit(FVoxelByteReader& InReader, FVoxelSectionCellEdit& OutEdit);
	static void WriteEntity(FVoxelByteWriter& InWriter, const FVoxelEntityWrite& InEdit);
	static bool ReadEntity(FVoxelByteReader& InReader, FVoxelEntityWrite& OutEdit);
	static bool ValidateWireKey(const FVoxelRepresentationWireKey& InKey);
};

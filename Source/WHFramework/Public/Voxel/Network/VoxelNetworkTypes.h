#pragma once

#include "CoreMinimal.h"
#include "Voxel/Generation/VoxelWorldManifest.h"
#include "Voxel/Interaction/VoxelEditTypes.h"
#include "Voxel/Save/VoxelDeltaTypes.h"
#include "VoxelNetworkTypes.generated.h"

class UPackageMap;

static constexpr uint32 VoxelProtocolVersion = 4;

USTRUCT()
struct WHFRAMEWORK_API FVoxelRPCPacket
{
	GENERATED_BODY()

	TArray<uint8> Bytes;
	bool NetSerialize(FArchive& InArchive, UPackageMap* InMap, bool& bOutSuccess);
};

template<>
struct TStructOpsTypeTraits<FVoxelRPCPacket> : TStructOpsTypeTraitsBase2<FVoxelRPCPacket>
{
	enum
	{
		WithNetSerializer = true
	};
};

UENUM()
enum class EVoxelMessage : uint8
{
	None = 0,
	Hello = 1,
	Welcome = 2,
	Ready = 3,
	Closed = 4,
	ChangeSummary = 5,
	SectionState = 6,
	SectionSnapshot = 7,
	SectionPatch = 8,
	Ack = 9,
	Resync = 10,
	Edit = 11,
	Reply = 12,
	Inventory = 13,
	RepresentationRequest = 14,
	RepresentationReply = 15,
	RepresentationInvalidate = 16
};

UENUM()
enum class EVoxelTransferPriority : uint8
{
	None = 0,
	Critical,
	Normal,
	Bulk
};

UENUM()
enum class EVoxelRepresentationWireType : uint8
{
	None = 0,
	VoxelProxy,
	SurfaceProxy,
	MacroTerrain
};

enum class EVoxelRepresentationReplyKind : uint8
{
	Retry = 0,
	Unchanged,
	Data
};

enum class EVoxelSectionWireState : uint8
{
	Unknown = 0,
	Natural,
	Modified
};

struct WHFRAMEWORK_API FVoxelWireMessage
{
	EVoxelMessage Kind = EVoxelMessage::None;
	FGuid Session;
	TArray<uint8> Payload;
};

struct WHFRAMEWORK_API FVoxelNetworkHello
{
	uint32 ProtocolVersion = VoxelProtocolVersion;
	uint64 Nonce = 0;
	uint64 RegistryHash = 0;
};

struct WHFRAMEWORK_API FVoxelNetworkWelcome
{
	uint64 EchoNonce = 0;
	FGuid Session;
	FVoxelWorldManifest Manifest;
};

struct WHFRAMEWORK_API FVoxelNetworkReady
{
	uint64 RecipeHash = 0;
	uint64 RegistryHash = 0;
	uint64 BaseSampleHash = 0;
};

struct WHFRAMEWORK_API FVoxelRegionChangeSummary
{
	FIntVector Region = FIntVector::ZeroValue;
	uint64 Revision = 0;
	TArray<uint64> ModifiedMask;
};

struct WHFRAMEWORK_API FVoxelSectionState
{
	FIntVector Section = FIntVector::ZeroValue;
	EVoxelSectionWireState State = EVoxelSectionWireState::Unknown;
	uint64 Revision = 0;
};

struct WHFRAMEWORK_API FVoxelNetworkSectionSnapshot
{
	FIntVector Section = FIntVector::ZeroValue;
	uint64 Revision = 0;
	TArray<FVoxelSectionCellEdit> Blocks;
	TArray<FVoxelEntityWrite> Entities;
};

struct WHFRAMEWORK_API FVoxelNetworkPatchBatch
{
	FGuid TransactionId;
	TArray<FVoxelSectionPatch> Sections;
};

struct WHFRAMEWORK_API FVoxelRepresentationWireKey
{
	FIntVector Coordinate = FIntVector::ZeroValue;
	uint8 Level = 0;

	bool operator==(const FVoxelRepresentationWireKey& InOther) const
	{
		return Coordinate == InOther.Coordinate && Level == InOther.Level;
	}
};

FORCEINLINE uint32 GetTypeHash(const FVoxelRepresentationWireKey& InKey)
{
	return HashCombineFast(::GetTypeHash(InKey.Coordinate), ::GetTypeHash(InKey.Level));
}

struct WHFRAMEWORK_API FVoxelRepresentationRequest
{
	EVoxelRepresentationWireType Type = EVoxelRepresentationWireType::None;
	FVoxelRepresentationWireKey Key;
	uint64 KnownRevision = 0;
	uint64 Serial = 0;
};

struct WHFRAMEWORK_API FVoxelRepresentationReply
{
	EVoxelRepresentationWireType Type = EVoxelRepresentationWireType::None;
	FVoxelRepresentationWireKey Key;
	uint64 Serial = 0;
	uint64 Revision = 0;
	EVoxelRepresentationReplyKind Kind = EVoxelRepresentationReplyKind::Retry;
	TArray<uint8> Data;
};

struct WHFRAMEWORK_API FVoxelRepresentationInvalidate
{
	TArray<FVoxelRepresentationWireKey> Keys;
	uint64 Revision = 0;
};

USTRUCT()
struct WHFRAMEWORK_API FVoxelNetworkSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Voxel|Network", meta = (ClampMin = "1024"))
	int32 BytesPerSecond = 131072;

	UPROPERTY(EditAnywhere, Category = "Voxel|Network", meta = (ClampMin = "4096"))
	int32 BurstBytes = 262144;

	UPROPERTY(EditAnywhere, Category = "Voxel|Network", meta = (ClampMin = "65536"))
	int32 MaxQueuedBytes = 8 * 1024 * 1024;

	UPROPERTY(EditAnywhere, Category = "Voxel|Network", meta = (ClampMin = "1"))
	int32 MaxRepresentationRequests = 64;

	bool Validate(FString& OutError) const;
};

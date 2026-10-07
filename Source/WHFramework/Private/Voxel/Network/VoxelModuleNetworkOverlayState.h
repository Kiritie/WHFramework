#pragma once

#include "CoreMinimal.h"
#include "Voxel/Network/VoxelOverlayAuthority.h"
#include "Voxel/Network/VoxelNetworkEnvelope.h"

struct FVoxelOverlayInboundWork final : FVoxelTaskCustomPayload
{
	EVoxelMessage Kind = EVoxelMessage::None;
	FVoxelOverlayRegionMessage Message;
	TArray<uint8> Bytes;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
	bool bServer = false;
	virtual uint64 GetAllocatedBytes() const override
	{
		return sizeof(*this) + Bytes.GetAllocatedSize();
	}
	virtual uint64 GetResourceAccountedBytes() const override
	{
		return ResourceLease ? ResourceLease->Bytes() : 0;
	}
};

struct FVoxelOverlayOutboundWork final : FVoxelTaskCustomPayload
{
	FVoxelOverlayEncodedMessage Message;
	TArray<uint8> WireBytes;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> WireLease;
	uint32 WireCrc = 0;
	int32 WireCrcOffset = 0;
	bool bBodyReady = false;
	bool bComplete = false;
	TAtomic<bool> bCapacityBlocked{false};
	TAtomic<uint64> CapacityVersion{0};
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
	virtual uint64 GetAllocatedBytes() const override
	{
		return sizeof(*this) + WireBytes.GetAllocatedSize() + Message.Bytes.GetAllocatedSize();
	}
	virtual uint64 GetResourceAccountedBytes() const override
	{
		return (ResourceLease ? ResourceLease->Bytes() : 0) + (WireLease ? WireLease->Bytes() : 0) +
			(Message.ResourceLease ? Message.ResourceLease->Bytes() : 0);
	}
};

struct FVoxelOverlayQueuedPacket
{
	FVoxelRPCPacket Packet;
	bool bServer = false;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
};

struct FVoxelOverlayNetworkState
{
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
	FVoxelOverlayAuthorityIdentity Identity;
	TArray<FIntVector> Regions;
	TArray<FIntVector> RetiringRegions;
	FGuid RetiringTicket;
	TSharedPtr<const FVoxelOverlayAuthorityDirectoryRoot, ESPMode::ThreadSafe> AuthorityRoot;
	TSharedPtr<const FVoxelOverlayAuthorityPrepared, ESPMode::ThreadSafe> Snapshot;
	TSharedPtr<FVoxelOverlaySnapshotEncoder, ESPMode::ThreadSafe> SnapshotEncoder;
	TSharedPtr<FVoxelOverlayDeltaEncoder, ESPMode::ThreadSafe> DeltaEncoder;
	TSharedPtr<const FVoxelOverlayAuthorityDelta, ESPMode::ThreadSafe> EncodingDelta;
	TArray<FVoxelOverlayAuthorityEditCapture> Edits;
	TArray<TSharedPtr<const FVoxelOverlayAuthorityDelta, ESPMode::ThreadSafe>> Replay;
	TMap<FIntVector, uint64> Acknowledged;
	TQueue<TSharedPtr<FVoxelOverlayInboundWork, ESPMode::ThreadSafe>> Inbound;
	TQueue<FVoxelOverlayQueuedPacket> Packets;
	TQueue<TSharedPtr<FVoxelNetworkEnvelopeDecode, ESPMode::ThreadSafe>> Envelopes;
	TSharedPtr<FVoxelNetworkEnvelopeDecode, ESPMode::ThreadSafe> EnvelopeHead;
	// 单个完成槽随连接元数据预授予；处理前保留正文、完整 Stamp 和耗时回执。
	TSharedPtr<FVoxelNetworkEnvelopeDecode, ESPMode::ThreadSafe> DecodedEnvelope;
	TOptional<FVoxelTaskResult> DecodedEnvelopeResult;
	FVoxelTaskStamp EnvelopeHeadStamp;
	double EnvelopeHeadFirstQueuedSeconds = 0.0;
	uint64 EnvelopeBytes = 0;
	bool bEnvelopeDecode = false;
	uint64 PacketBytes = 0;
	TSharedPtr<FVoxelOverlayOutboundWork, ESPMode::ThreadSafe> Outbound;
	uint64 InboundBytes = 0;
	uint64 ReplayBytes = 0;
	uint64 BuildSerial = 0;
	FVoxelTaskStamp BuildStamp;
	FVoxelTaskStamp EncodeStamp;
	uint64 SeenCurrentRevision = MAX_uint64;
	uint64 SeenDesiredRevision = MAX_uint64;
	uint64 LastProgress = 0;
	double LastControl = -10.0;
	double LastRetry = -10.0;
	int32 RetryCount = 0;
	bool bBuild = false;
	bool bDecode = false;
	bool bEncode = false;
	bool bNeedsSnapshot = false;
};

#pragma once

#include "CoreMinimal.h"
#include "Voxel/Network/VoxelNetworkCodec.h"
#include "Voxel/Network/VoxelOverlayReplica.h"

// 完整传输正文保持原输入租约。GT 不计算正文 CRC，不复制或解析 Envelope。
class WHFRAMEWORK_API FVoxelNetworkEnvelopeDecode final : public FVoxelTaskCustomPayload,
	public TSharedFromThis<FVoxelNetworkEnvelopeDecode, ESPMode::ThreadSafe>
{
public:
	static TSharedPtr<FVoxelNetworkEnvelopeDecode, ESPMode::ThreadSafe> Create(
		TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
		const FVoxelGenerationConsumerKey& InConsumer, TArray<uint8>&& InBytes,
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InInputLease,
		uint32 InCrc, bool bInServer, FString& OutError);
	FVoxelTaskResult Advance(const TAtomic<bool>& InCancel);
	EVoxelTaskBlockReason GetBlockReason() const;
	uint64 GetIngressQuotaBytes() const;
	const FVoxelWireMessage& GetMessage() const { return Message; }
	FVoxelOverlayRegionMessage TakeOverlay() { return MoveTemp(Overlay); }
	bool IsOverlay() const { return bOverlay; }
	bool IsServer() const { return bServer; }
	bool IsRepresentationReply() const { return bRepresentationReply; }
	const FVoxelRepresentationReply& GetRepresentationReply() const { return RepresentationReply; }
	virtual uint64 GetAllocatedBytes() const override;
	virtual uint64 GetResourceAccountedBytes() const override;

private:
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	FVoxelGenerationConsumerKey Consumer;
	TArray<uint8> Bytes;
	FVoxelWireMessage Message;
	FVoxelOverlayRegionMessage Overlay;
	FVoxelRepresentationReply RepresentationReply;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InputLease;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> MetadataLease;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> DecodeLease;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> OverlayLease;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> RepresentationLease;
	uint64 DecodeBytes = 0;
	uint64 IngressQuotaBytes = 0;
	uint64 CapacityVersion = 0;
	uint32 ExpectedCrc = 0;
	uint32 RunningCrc = 0;
	int32 VerifiedBytes = 0;
	bool bWaitingCapacity = false;
	bool bServer = false;
	bool bOverlay = false;
	bool bRepresentationReply = false;
	bool bDecoded = false;
	bool bComplete = false;
};

#pragma once

#include "CoreMinimal.h"
#include "Voxel/Network/VoxelOverlayAuthority.h"
#include "Voxel/Network/VoxelNetworkEnvelope.h"
#include "Voxel/Task/VoxelTaskScheduler.h"

// 原 owner 根内的接纳槽；拒收时保留已绑定函数、完整尝试及其原租约。
struct FVoxelOverlayTaskAdmission
{
	TOptional<FVoxelTaskRequest> Request;
	uint64 CallbackVersion = 0;
	bool bCallbackCapacityBlocked = false;

	template<typename BlockFunctor, typename ExecuteFunctor, typename ApplyFunctor>
	bool Bind(TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
		const FVoxelGenerationConsumerKey& InConsumer, BlockFunctor&& InBlock,
		ExecuteFunctor&& InExecute, ApplyFunctor&& InApply, FString& OutError)
	{
		if (!Request.IsSet())
		{
			OutError = TEXT("InvalidConsumer: overlay callback admission has no original request");
			return false;
		}
		if (bCallbackCapacityBlocked)
		{
			uint64 Current = 0;
			if (!InResources->TryGetReleaseVersionExcludingOwner(InConsumer, Current) || Current <= CallbackVersion)
			{
				OutError = TEXT("CapacityBlocked: overlay callback admission awaits external release");
				return false;
			}
			bCallbackCapacityBlocked = false;
		}
		const uint64 BeforeBinding = InResources->ReleaseVersionExcludingOwner(InConsumer);
		if ((!Request->bBlockBound && !Request->BindGetBlockReason(InResources, InConsumer, Forward<BlockFunctor>(InBlock), OutError)) ||
			(!Request->bExecuteBound && !Request->BindExecute(InResources, InConsumer, Forward<ExecuteFunctor>(InExecute), OutError)) ||
			(!Request->bApplyBound && !Request->BindApply(InResources, InConsumer, Forward<ApplyFunctor>(InApply), OutError)))
		{
			bCallbackCapacityBlocked = OutError.StartsWith(TEXT("CapacityBlocked"));
			CallbackVersion = BeforeBinding;
			return false;
		}
		OutError.Reset();
		return true;
	}

	void Reset()
	{
		Request.Reset();
		CallbackVersion = 0;
		bCallbackCapacityBlocked = false;
	}

	bool Enqueue(TFunctionRef<bool(FVoxelTaskRequest&&)> InQueue,
		TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
		const FVoxelGenerationConsumerKey& InConsumer, FString& OutError)
	{
		check(Request.IsSet());
		const uint64 BeforeAdmission = InResources->ReleaseVersionExcludingOwner(InConsumer);
		FVoxelTaskRequest Candidate = MoveTemp(Request.GetValue());
		Request.Reset();
		if (InQueue(MoveTemp(Candidate)))
		{
			OutError.Reset();
			return true;
		}
		if (Request.IsSet()) return false;
		Request.Emplace(MoveTemp(Candidate));
		if (Request->Blocked == EVoxelTaskBlockReason::Oversized || Request->Blocked == EVoxelTaskBlockReason::TerminalFailure)
		{
			OutError = TEXT("Oversized: overlay original task admission is unreachable or invalid");
			return false;
		}
		bCallbackCapacityBlocked = (uint16(Request->Blocked) & (uint16(EVoxelTaskBlockReason::Memory) | uint16(EVoxelTaskBlockReason::InputMemory))) != 0;
		CallbackVersion = BeforeAdmission;
		OutError.Reset();
		return false;
	}
};

struct FVoxelOverlayInboundWork final : FVoxelTaskCustomPayload
{
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
	EVoxelMessage Kind = EVoxelMessage::None;
	FVoxelOverlayRegionMessage Message;
	TArray<uint8> Bytes;
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
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> WireLease;
	FVoxelOverlayEncodedMessage Message;
	TArray<uint8> WireBytes;
	uint32 WireCrc = 0;
	int32 WireCrcOffset = 0;
	bool bBodyReady = false;
	bool bComplete = false;
	TAtomic<bool> bCapacityBlocked{false};
	TAtomic<uint64> CapacityVersion{0};
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
	FVoxelOverlayTaskAdmission EnvelopeAdmission;
	FVoxelOverlayTaskAdmission DecodeAdmission;
	FVoxelOverlayTaskAdmission EncodeAdmission;
	FVoxelOverlayTaskAdmission BuildAdmission;
	TSharedPtr<FVoxelOverlayInboundWork, ESPMode::ThreadSafe> InboundHead;
	TSharedPtr<FVoxelOverlayOutboundWork, ESPMode::ThreadSafe> EncodeHead;
	TSharedPtr<FVoxelOverlayAuthoritySnapshotBuild, ESPMode::ThreadSafe> SnapshotBuildHead;
	TSharedPtr<FVoxelOverlayAuthorityDeltaBuild, ESPMode::ThreadSafe> DeltaBuildHead;
	TSharedPtr<const FVoxelOverlayAuthorityDirectoryRoot, ESPMode::ThreadSafe> DeltaBuildBasis;
	FVoxelOverlayAuthorityIdentity SnapshotBuildIdentity;
	FVoxelGenerationConsumerKey EncodeConsumer;
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

#pragma once

#include "CoreMinimal.h"
#include "Voxel/Network/VoxelOverlayAuthority.h"
#include "Voxel/Network/VoxelNetworkEnvelope.h"
#include "Voxel/Task/VoxelTaskScheduler.h"

// 原连接单线程队列；节点真实布局公开计数，无隐式 sentinel 分配。
template<typename ValueType>
class TVoxelNetworkQueue
{
	struct FNode
	{
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
		FNode* Next = nullptr;
		ValueType Value;

		FNode(TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InLease, ValueType&& InValue)
			: ResourceLease(MoveTemp(InLease))
			, Value(MoveTemp(InValue))
		{
		}
	};

public:
	TVoxelNetworkQueue() = default;
	~TVoxelNetworkQueue()
	{
		Empty();
	}
	TVoxelNetworkQueue(const TVoxelNetworkQueue&) = delete;
	TVoxelNetworkQueue& operator=(const TVoxelNetworkQueue&) = delete;
	static uint64 NodeBytes() { return FMemory::QuantizeSize(sizeof(FNode), alignof(FNode)); }
	bool IsEmpty() const { return Head == nullptr; }
	bool Enqueue(ValueType&& InValue, TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InLease)
	{
		check(IsInGameThread());
		if (!InLease || InLease->GetPool() != EVoxelTerrainResourcePool::Input || InLease->Bytes() != NodeBytes()) return false;
		auto* Node = new (FMemory::Malloc(NodeBytes(), alignof(FNode))) FNode(MoveTemp(InLease), MoveTemp(InValue));
		if (Tail) Tail->Next = Node;
		else Head = Node;
		Tail = Node;
		Node->ResourceLease->MarkPublished();
		return true;
	}
	bool Dequeue(ValueType& OutValue)
	{
		check(IsInGameThread());
		if (!Head) return false;
		FNode* Node = Head;
		Head = Node->Next;
		if (!Head) Tail = nullptr;
		OutValue = MoveTemp(Node->Value);
		Free(Node);
		return true;
	}
	void Empty()
	{
		while (Head)
		{
			FNode* Node = Head;
			Head = Node->Next;
			Free(Node);
		}
		Tail = nullptr;
	}

private:
	static void Free(FNode* InNode)
	{
		auto Lease = MoveTemp(InNode->ResourceLease);
		InNode->~FNode();
		FMemory::Free(InNode);
		Lease.Reset();
	}
	FNode* Head = nullptr;
	FNode* Tail = nullptr;
};

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
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
	FVoxelRPCPacket Packet;
	FVoxelGenerationConsumerKey Consumer;
	bool bServer = false;
	FVoxelOverlayQueuedPacket() = default;
	FVoxelOverlayQueuedPacket(FVoxelOverlayQueuedPacket&&) = default;
	FVoxelOverlayQueuedPacket& operator=(FVoxelOverlayQueuedPacket&& InOther)
	{
		if (this == &InOther) return *this;
		Packet.Bytes = MoveTemp(InOther.Packet.Bytes);
		ResourceLease = MoveTemp(InOther.ResourceLease);
		Consumer = InOther.Consumer;
		bServer = InOther.bServer;
		return *this;
	}
	FVoxelOverlayQueuedPacket(const FVoxelOverlayQueuedPacket&) = delete;
	FVoxelOverlayQueuedPacket& operator=(const FVoxelOverlayQueuedPacket&) = delete;
};

// 原连接唯一控制尝试；标量随 NetworkState native body 预授予，无重试队列。
struct FVoxelOverlayControlAdmission
{
	FVoxelGenerationConsumerKey Consumer;
	FVoxelOverlayAuthorityIdentity Identity;
	FVoxelTerrainResourceLimits Limits;
	EVoxelMessage Kind = EVoxelMessage::None;
	EVoxelTerrainResourceAdmission Admission = EVoxelTerrainResourceAdmission::None;
	uint64 CapacityVersion = 0;
};

struct FVoxelOverlayNetworkState
{
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
	FVoxelOverlayAuthorityIdentity Identity;
	TArray<FIntVector> Regions;
	TArray<FIntVector> RetiringRegions;
	FGuid RetiringTicket;
	FVoxelOverlayControlAdmission ControlAdmission;
	FVoxelOverlayControlAdmission SubscriptionAdmission;
	FVoxelOverlayControlAdmission SnapshotFactoryAdmission;
	FVoxelOverlayControlAdmission DeltaFactoryAdmission;
	FVoxelOverlayControlAdmission EncoderFactoryAdmission;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> EncoderFactoryHeadLease;
	TArray<FVoxelOverlayRegionHead> EncoderFactoryHeads;
	FVoxelOverlayControlAdmission OutboundFactoryAdmission;
	bool bEncoderDeltaCommitPending = false;
	FGuid DeltaFactoryTransaction;
	uint64 SubscriptionCurrentDemandRevision = MAX_uint64;
	uint64 SubscriptionDesiredDemandRevision = MAX_uint64;
	uint64 SubscriptionCurrentResidencyRevision = MAX_uint64;
	uint64 SubscriptionDesiredResidencyRevision = MAX_uint64;
	bool bSubscriptionAttempt = false;
	bool bSubscriptionExpected = false;
	TSharedPtr<const FVoxelOverlayAuthorityDirectoryRoot, ESPMode::ThreadSafe> AuthorityRoot;
	TSharedPtr<const FVoxelOverlayAuthorityPrepared, ESPMode::ThreadSafe> Snapshot;
	TSharedPtr<FVoxelOverlaySnapshotEncoder, ESPMode::ThreadSafe> SnapshotEncoder;
	TSharedPtr<FVoxelOverlayDeltaEncoder, ESPMode::ThreadSafe> DeltaEncoder;
	TSharedPtr<const FVoxelOverlayAuthorityDelta, ESPMode::ThreadSafe> EncodingDelta;
	TArray<FVoxelOverlayAuthorityEditCapture> Edits;
	TArray<TSharedPtr<const FVoxelOverlayAuthorityDelta, ESPMode::ThreadSafe>> Replay;
	TMap<FIntVector, uint64> Acknowledged;
	TVoxelNetworkQueue<TSharedPtr<FVoxelOverlayInboundWork, ESPMode::ThreadSafe>> Inbound;
	TVoxelNetworkQueue<FVoxelOverlayQueuedPacket> Packets;
	TVoxelNetworkQueue<TSharedPtr<FVoxelNetworkEnvelopeDecode, ESPMode::ThreadSafe>> Envelopes;
	TOptional<FVoxelOverlayQueuedPacket> PacketHead;
	uint64 PacketHeadCapacityVersion = 0;
	bool bPacketHeadWaitingCapacity = false;
	FVoxelGenerationConsumerKey IngressCapacityConsumer;
	uint64 IngressCapacityVersion = 0;
	bool bIngressResyncPending = false;
	bool bIngressWaitingCapacity = false;
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
	FVoxelTerrainResourceLimits SnapshotBuildLimits;
	FVoxelOverlayAuthorityIdentity SnapshotTerminalIdentity;
	FVoxelTerrainResourceLimits SnapshotTerminalLimits;
	FVoxelGenerationConsumerKey SnapshotTerminalConsumer;
	bool bSnapshotTerminalFailure = false;
	FVoxelGenerationConsumerKey EncodeConsumer;
	// 单个完成槽随连接元数据预授予；处理前保留正文、完整 Stamp 和耗时回执。
	TSharedPtr<FVoxelNetworkEnvelopeDecode, ESPMode::ThreadSafe> DecodedEnvelope;
	TOptional<FVoxelTaskResult> DecodedEnvelopeResult;
	// 原完成槽保留通知正文和游标；数组先销毁，随后归还原输入授额。
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> InvalidationLease;
	FVoxelRepresentationInvalidate Invalidation;
	FVoxelGenerationConsumerKey InvalidationCapacityConsumer;
	uint64 InvalidationCapacityVersion = 0;
	int32 InvalidationCursor = 0;
	bool bInvalidationDecoded = false;
	bool bInvalidationWaitingCapacity = false;
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
	uint64 SeenCurrentDemandRevision = MAX_uint64;
	uint64 SeenDesiredDemandRevision = MAX_uint64;
	uint64 SeenCurrentResidencyRevision = MAX_uint64;
	uint64 SeenDesiredResidencyRevision = MAX_uint64;
	uint64 LastProgress = 0;
	double LastControl = -10.0;
	double LastRetry = -10.0;
	int32 RetryCount = 0;
	bool bBuild = false;
	bool bDecode = false;
	bool bEncode = false;
	bool bNeedsSnapshot = false;
};

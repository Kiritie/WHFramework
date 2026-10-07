#include "Voxel/Network/VoxelModuleNetworkComponent.h"
#include "VoxelModuleNetworkOverlayState.h"
#include "VoxelServerRepresentationBuild.h"
#include "VoxelRepresentationSubscriptionState.h"
#include "Voxel/Rendering/VoxelViewLod.h"

#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Network/VoxelNetworkCodec.h"
#include "Voxel/Network/VoxelRepresentationSync.h"
#include "Voxel/Runtime/VoxelWorldRuntime.h"
#include "Voxel/Serialization/VoxelBinaryCodec.h"
#include "Voxel/Save/VoxelDeltaCodec.h"
#include "Voxel/Save/VoxelRegionStore.h"
#include "Voxel/Network/VoxelOverlayReplica.h"
#include "Voxel/Streaming/VoxelEmergeManager.h"
#include "Voxel/Streaming/VoxelInterestManager.h"
#include "Voxel/Task/VoxelTerrainPriority.h"
#include "Voxel/VoxelModule.h"
#include "Async/Async.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"

bool UVoxelModuleNetworkComponent::IsRepresentationInRange(
	const FVector& InObserverCell, const EVoxelRepresentationWireType InType,
	const FVoxelGenerationBounds& InBounds, const FVoxelViewSettings& InSettings)
{
	FVoxelStreamingSource Source;
	Source.View = InSettings.DefaultSourceView;
	const FVoxelResolvedViewRanges Ranges = VoxelViewLod::ResolveViewRanges(Source);
	const double Radius = InType == EVoxelRepresentationWireType::VoxelProxy ? Ranges.Proxy :
		(InType == EVoxelRepresentationWireType::SurfaceProxy ? Ranges.Surface : Ranges.Macro);
	const FVector Nearest = FBox(FVector(InBounds.Min), FVector(InBounds.Max)).GetClosestPointTo(InObserverCell);
	return FVector2D::DistSquared(FVector2D(InObserverCell), FVector2D(Nearest)) <= FMath::Square(Radius) &&
		(InType != EVoxelRepresentationWireType::VoxelProxy || FMath::Abs(InObserverCell.Z - Nearest.Z) <= Source.View.VoxelProxyVerticalHalfExtentCells);
}

UVoxelModuleNetworkComponent::UVoxelModuleNetworkComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

UVoxelModuleNetworkComponent::~UVoxelModuleNetworkComponent() = default;

void UVoxelModuleNetworkComponent::BeginPlay()
{
	Super::BeginPlay();
	Transfer.SetSettings(NetworkSettings);
	BindModule();
}

void UVoxelModuleNetworkComponent::EndPlay(const EEndPlayReason::Type InReason)
{
	if (UVoxelModule* VoxelModule = Module.Get())
	{
		VoxelModule->OnBlocksCommitted.Remove(CommitHandle);
		VoxelModule->UnregisterTerrainNetwork(this);
		if (SourceId.IsValid())
		{
			VoxelModule->UnregisterSource(SourceId);
		}
	}
	ResetProtocol();
	Super::EndPlay(InReason);
}

void UVoxelModuleNetworkComponent::TickComponent(
	const float InDeltaSeconds,
	const ELevelTick InTickType,
	FActorComponentTickFunction* InFunction)
{
	Super::TickComponent(InDeltaSeconds, InTickType, InFunction);
	if (!Module.IsValid())
	{
		BindModule();
	}
	UVoxelModule* VoxelModule = Module.Get();
	if (VoxelModule && !VoxelModule->GetTerrainFrameContext().IsActive()) AdvanceDeferredWorldControl();
	if (!VoxelModule || bRejected)
	{
		return;
	}
	const double Now = FPlatformTime::Seconds();
	// 世界就绪后所有协议、传输和重试工作都由 Module 的同一个 FrameContext 推进。
	if (VoxelModule->IsReady()) return;
	Transfer.Tick(Now, [this](const TArray<uint8>& InFragment)
	{
		FVoxelRPCPacket Packet;
		Packet.Bytes = InFragment;
		if (GetOwner() && GetOwner()->HasAuthority())
		{
			ClientReceive(Packet);
		}
		else
		{
			ServerReceive(Packet);
		}
	});

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		if (bReady && Now - LastInterest >= 0.25)
		{
			RefreshInterest(Now);
		}
	}
	else if (!bWelcomed && Now - LastHello >= 1.0)
	{
		if (ClientNonce == 0)
		{
			ClientNonce = FMath::Max<uint64>(1, FPlatformTime::Cycles64());
		}
		FVoxelNetworkHello Hello;
		Hello.Nonce = ClientNonce;
		Hello.RegistryHash = VoxelModule->GetRegistry() ? VoxelModule->GetRegistry()->Hash : 0;
		TArray<uint8> Payload;
		if (FVoxelNetworkCodec::EncodeHello(Hello, Payload))
		{
			Send(EVoxelMessage::Hello, Payload, EVoxelTransferPriority::Critical);
		}
		LastHello = Now;
	}
}

void UVoxelModuleNetworkComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UVoxelModuleNetworkComponent, InventoryPawn);
	DOREPLIFETIME(UVoxelModuleNetworkComponent, InventoryEpoch);
}

uint64 UVoxelModuleNetworkComponent::AllocateRequestId()
{
	return NextOutboundId == MAX_uint64 ? 0 : NextOutboundId++;
}

bool UVoxelModuleNetworkComponent::SubmitIntent(const FVoxelEditIntent& InIntent)
{
	if (!IsSessionReady() || InIntent.RequestId == 0)
	{
		return false;
	}
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		UVoxelModule* VoxelModule = Module.Get();
		if (!VoxelModule)
		{
			return false;
		}
		const FVoxelEditReply Reply = VoxelModule->ExecuteIntent(
			Controller(),
			GetAuthorizedObserver(),
			InIntent,
			false);
		OnIntentReply.Broadcast(Reply);
		return true;
	}
	TArray<uint8> Payload;
	return FVoxelNetworkCodec::EncodeIntent(InIntent, Payload) &&
		Send(EVoxelMessage::Edit, Payload, EVoxelTransferPriority::Critical);
}

bool UVoxelModuleNetworkComponent::RequestRepresentation(
	const EVoxelRepresentationWireType InType,
	const FVoxelRepresentationWireKey& InKey,
	const uint64 InKnownRevision)
{
	const auto* VoxelModule = Module.Get();
	if (!VoxelModule || !OverlayNetwork) return false;
	const FVoxelTerrainSpatialKey SpatialKey{static_cast<EVoxelTerrainRepresentation>(static_cast<uint8>(InType) + 1), InKey.Coordinate, InKey.Level};
	const auto* Geometry = VoxelModule->GetCurrentInterest().Geometry.Find(SpatialKey);
	if (!Geometry) return false;

	if (!IsSessionReady() || !GetOwner() || GetOwner()->HasAuthority() ||
		PendingRepresentationRequests.Num() >= FMath::Min(NetworkSettings.MaxRepresentationRequests, RepresentationTableCapacity))
	{
		return false;
	}
	for (const auto& Pair : PendingRepresentationRequests)
	{
		if (Pair.Value.Request.Type == InType && Pair.Value.Request.Key == InKey && Pair.Value.Request.Geometry == *Geometry)
		{
			return true;
		}
	}
	FVoxelRepresentationRequest Request;
	Request.Type = InType;
	Request.Key = InKey;
	Request.KnownRevision = InKnownRevision;
	Request.Geometry = *Geometry;
	Request.Serial = AllocateRequestId();
	TArray<uint8> Payload;
	if (Request.Serial == 0 || !FVoxelNetworkCodec::EncodeRepresentationRequest(Request, Payload) ||
		!Send(EVoxelMessage::RepresentationRequest, Payload, EVoxelTransferPriority::Normal))
	{
		return false;
	}
	FPendingRepresentation Pending;
	Pending.Request = Request; Pending.RequestSession = Session;
	Pending.WorldEpoch = VoxelModule->GetWorldEpoch(); Pending.OwnerIncarnation = TaskOwnerIncarnation;
	Pending.LastSentSeconds = FPlatformTime::Seconds();
	PendingRepresentationRequests.Add(Request.Serial, MoveTemp(Pending));
	return true;
}

bool UVoxelModuleNetworkComponent::IsSessionReady() const
{
	return bReady && !bRejected;
}

void UVoxelModuleNetworkComponent::SetAuthorizedObserver(
	AActor* InObserver,
	const bool bInAllowBuild)
{
	AuthorizedObserver = InObserver;
	bObserverCanBuild = bInAllowBuild;
}

AActor* UVoxelModuleNetworkComponent::GetAuthorizedObserver() const
{
	if (AuthorizedObserver.IsValid())
	{
		return AuthorizedObserver.Get();
	}
	const APlayerController* PlayerController = Controller();
	return PlayerController ? PlayerController->GetPawn() : nullptr;
}

void UVoxelModuleNetworkComponent::PushInventoryFor(AActor* InOwner)
{
	(void)InOwner;
}

void UVoxelModuleNetworkComponent::ServerReceive_Implementation(const FVoxelRPCPacket& InPacket)
{
	Receive(InPacket, true);
}

void UVoxelModuleNetworkComponent::ClientReceive_Implementation(const FVoxelRPCPacket& InPacket)
{
	Receive(InPacket, false);
}

void UVoxelModuleNetworkComponent::BindModule()
{
	UVoxelModule* VoxelModule = UVoxelModule::GetPtr(GetWorld());
	if (!VoxelModule)
	{
		return;
	}
	if (Module.Get() == VoxelModule)
	{
		return;
	}
	if (UVoxelModule* Old = Module.Get())
	{
		Old->OnBlocksCommitted.Remove(CommitHandle);
		Old->UnregisterTerrainNetwork(this);
	}
	Module = VoxelModule;
	VoxelModule->RegisterTerrainNetwork(this);
	CommitHandle = VoxelModule->OnBlocksCommitted.AddUObject(this, &UVoxelModuleNetworkComponent::OnCommit);
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		Session = VoxelModule->GetSessionId();
	}
}

void UVoxelModuleNetworkComponent::ResetProtocol()
{
	check(TaskOwnerIncarnation < MAX_uint64);
	++TaskOwnerIncarnation;
	Transfer.Reset();
	OverlayNetwork.Reset();
	GameplayInterest.Empty();
	ResetRepresentationRequests();
	Session.Invalidate();
	SourceId.Invalidate();
	ClientNonce = 0;
	NextOutboundId = 1;
	LastInboundEditId = 0;
	LastHello = -10.0;
	LastInterest = -10.0;
	bReady = false;
	bWelcomed = false;
	bRejected = false;
}

void UVoxelModuleNetworkComponent::OnTerrainWorldStopping()
{
	ResetProtocol();
}

bool UVoxelModuleNetworkComponent::AdvanceDeferredWorldControl()
{
	auto* WorldModule = Module.Get();
	if (!WorldModule || WorldModule->GetTerrainFrameContext().IsActive()) return false;
	return ApplyDecodedEnvelope(true);
}

void UVoxelModuleNetworkComponent::Fail(const FString& InReason)
{
	if (bRejected)
	{
		return;
	}
	bRejected = true;
	bReady = false;
	OnProtocolFailure.Broadcast(InReason);
}

bool UVoxelModuleNetworkComponent::Send(
	const EVoxelMessage InKind,
	const TArray<uint8>& InPayload,
	const EVoxelTransferPriority InPriority)
{
	TArray<uint8> Message;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Scratch;
	if (OverlayNetwork)
	{
		const uint64 Bytes = VoxelBoundedAllocation::InitialGrowBytes<uint8>(InPayload.Num() + 29);
		const auto Consumer = FVoxelGenerationConsumerKey::Section(Module->GetWorldEpoch(), TaskOwnerId,
			TaskOwnerIncarnation, {}, ++NextTaskSerial);
		if (!OverlayNetwork->Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Staged, Bytes, Lease) ||
			!OverlayNetwork->Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Working, Bytes, Scratch)) return false;
	}
	const FGuid EnvelopeSession = InKind == EVoxelMessage::Hello ? FGuid() : Session;
	if (!FVoxelNetworkCodec::Encode(InKind, EnvelopeSession, InPayload, Message)) return false;
	if (Lease)
	{
		if (uint64(Message.GetAllocatedSize()) > Lease->Bytes()) return false;
		Lease->Resize(Message.GetAllocatedSize());
	}
	return Transfer.Enqueue(InPriority, MoveTemp(Message), MoveTemp(Lease));
}

void UVoxelModuleNetworkComponent::Receive(const FVoxelRPCPacket& InPacket, const bool bInServer)
{
	if (!OverlayNetwork || !Module.IsValid() || !Module->IsReady()) { ProcessPacket(InPacket, bInServer); return; }
	const uint64 Bytes = VoxelBoundedAllocation::ArrayBytes<uint8>(InPacket.Bytes.Num()) + sizeof(FVoxelOverlayQueuedPacket) + sizeof(void*);
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	if (InPacket.Bytes.Num() > 4608 || OverlayNetwork->PacketBytes + Bytes > 32ull * 1024 * 1024 ||
		!OverlayNetwork->Resources->Acquire(FVoxelGenerationConsumerKey::Section(Module->GetWorldEpoch(), TaskOwnerId,
			TaskOwnerIncarnation, {}, ++NextTaskSerial), EVoxelTerrainResourcePool::Input, Bytes, Lease))
	{
		Fail(TEXT("Voxel transport receive capacity exceeded"));
		return;
	}
	FVoxelOverlayQueuedPacket Queued;
	Queued.bServer = bInServer;
	Queued.ResourceLease = MoveTemp(Lease);
	Queued.Packet.Bytes.Reserve(InPacket.Bytes.Num());
	Queued.Packet.Bytes.Append(InPacket.Bytes);
	OverlayNetwork->PacketBytes += Bytes;
	OverlayNetwork->Packets.Enqueue(MoveTemp(Queued));
}

void UVoxelModuleNetworkComponent::ProcessPacket(const FVoxelRPCPacket& InPacket, const bool bInServer)
{
	const double Now = FPlatformTime::Seconds();
	if (Now - PacketWindow >= 1.0)
	{
		PacketWindow = Now;
		PacketsInWindow = 0;
	}
	if (++PacketsInWindow > 256)
	{
		Fail(TEXT("Voxel network packet rate exceeded"));
		return;
	}
	TArray<uint8> Completed;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> CompletedLease;
	uint32 Crc = 0;
	const bool bWorkerDecode = OverlayNetwork && Module.IsValid() && Module->IsReady();
	const bool bAssembled = bWorkerDecode ? Transfer.Assemble(InPacket.Bytes, Now, Completed, Crc, &CompletedLease) :
		Transfer.Receive(InPacket.Bytes, Now, Completed, &CompletedLease);
	if (!bAssembled)
	{
		Fail(TEXT("Malformed voxel transfer fragment"));
		return;
	}
	if (Completed.IsEmpty())
	{
		return;
	}
	if (bWorkerDecode)
	{
		FString Error;
		const auto Consumer = FVoxelGenerationConsumerKey::Section(Module->GetWorldEpoch(), TaskOwnerId,
			TaskOwnerIncarnation, {}, ++NextTaskSerial);
		auto Work = FVoxelNetworkEnvelopeDecode::Create(OverlayNetwork->Resources.ToSharedRef(), Consumer,
			MoveTemp(Completed), MoveTemp(CompletedLease), Crc, bInServer, Error);
		if (!Work) { Fail(Error); return; }
		const uint64 Quota = Work->GetIngressQuotaBytes();
		if (Quota + PendingRepresentationBytes + OverlayNetwork->EnvelopeBytes + OverlayNetwork->PacketBytes + OverlayNetwork->InboundBytes > 32ull * 1024 * 1024)
		{
			Fail(TEXT("完整 Envelope 超过连接接收配额"));
			return;
		}
		OverlayNetwork->EnvelopeBytes += Quota;
		OverlayNetwork->Envelopes.Enqueue(MoveTemp(Work));
		return;
	}
	FVoxelWireMessage Message;
	if (!FVoxelNetworkCodec::Decode(Completed, Message))
	{
		Fail(TEXT("Malformed voxel V4 envelope"));
		return;
	}
	if (Message.Kind != EVoxelMessage::Hello && Message.Kind != EVoxelMessage::Welcome &&
		bWelcomed && Message.Session != Session)
	{
		Fail(TEXT("Voxel session identity mismatch"));
		return;
	}
	Handle(Message, bInServer);
}

void UVoxelModuleNetworkComponent::AdvanceEnvelopeDecode(const double InNow)
{
	auto* WorldModule = Module.Get();
	if (!OverlayNetwork || !WorldModule || OverlayNetwork->bEnvelopeDecode) return;
	if (!OverlayNetwork->EnvelopeHead)
	{
		if (!OverlayNetwork->Envelopes.Dequeue(OverlayNetwork->EnvelopeHead)) return;
		OverlayNetwork->EnvelopeHeadFirstQueuedSeconds = InNow;
		OverlayNetwork->EnvelopeHeadStamp.WorldEpoch = WorldModule->GetWorldEpoch();
		OverlayNetwork->EnvelopeHeadStamp.OwnerId = TaskOwnerId;
		OverlayNetwork->EnvelopeHeadStamp.OwnerIncarnation = TaskOwnerIncarnation;
		check(NextTaskSerial != MAX_uint64);
		OverlayNetwork->EnvelopeHeadStamp.Token = ++NextTaskSerial;
	}
	const auto Work = OverlayNetwork->EnvelopeHead;
	FVoxelTaskRequest Task;
	Task.Kind = EVoxelTaskKind::NetworkRepresentation;
	Task.Stamp = OverlayNetwork->EnvelopeHeadStamp;
	Task.WorkClass = EVoxelWorkClass::Visible;
	Task.ServiceBand = EVoxelTerrainServiceBand::P2;
	Task.SourceStableKey = TaskOwnerId;
	Task.FirstRunnableSeconds = OverlayNetwork->EnvelopeHeadFirstQueuedSeconds;
	Task.InputBytes = Work->GetResourceAccountedBytes();
	Task.ReservedBytes = 1;
	Task.bResumable = true;
	Task.GetBlockReason = [Work] { return Work->GetBlockReason(); };
	Task.Execute = [Work](const TAtomic<bool>& Cancel) { return Work->Advance(Cancel); };
	Task.Apply = [Weak = TWeakObjectPtr<UVoxelModuleNetworkComponent>(this), Work](FVoxelTaskResult&& Result)
	{
		if (auto* Component = Weak.Get()) Component->StageDecodedEnvelope(Work, MoveTemp(Result));
	};
	if (WorldModule->EnqueueNetworkRepresentationTask(MoveTemp(Task)))
	{
		OverlayNetwork->bEnvelopeDecode = true;
		OverlayNetwork->EnvelopeHead.Reset();
	}
	// 容量等待保持队首、尝试身份与排队时间，后续消息不能越过它。
}

bool UVoxelModuleNetworkComponent::StageDecodedEnvelope(
	const TSharedPtr<FVoxelNetworkEnvelopeDecode, ESPMode::ThreadSafe>& InWork,
	FVoxelTaskResult&& InResult)
{
	check(IsInGameThread());
	if (!InWork || !OverlayNetwork || !Module.IsValid() || bRejected ||
		TaskOwnerId != InResult.Stamp.OwnerId || TaskOwnerIncarnation != InResult.Stamp.OwnerIncarnation ||
		Module->GetWorldEpoch() != InResult.Stamp.WorldEpoch ||
		!OverlayNetwork->bEnvelopeDecode || !(OverlayNetwork->EnvelopeHeadStamp == InResult.Stamp) ||
		OverlayNetwork->DecodedEnvelope) return false;
	// Scheduler Apply 只转移引用，不触发协议回调或世界关闭。
	OverlayNetwork->DecodedEnvelope = InWork;
	OverlayNetwork->DecodedEnvelopeResult.Emplace(MoveTemp(InResult));
	if (!InWork->IsServer() && InWork->GetMessage().Kind == EVoxelMessage::Welcome)
		Module->QueueTerrainWorldControl(this);
	return true;
}

bool UVoxelModuleNetworkComponent::ApplyDecodedEnvelope(const bool bInWorldControl)
{
	check(IsInGameThread());
	auto* WorldModule = Module.Get();
	if (!WorldModule || !OverlayNetwork || !OverlayNetwork->DecodedEnvelope ||
		!OverlayNetwork->DecodedEnvelopeResult.IsSet()) return false;
	const bool bWorldControl = !OverlayNetwork->DecodedEnvelope->IsServer() &&
		OverlayNetwork->DecodedEnvelope->GetMessage().Kind == EVoxelMessage::Welcome;
	if (bWorldControl != bInWorldControl || (bWorldControl && WorldModule->GetTerrainFrameContext().IsActive())) return false;

	// Handle 可在帧外关闭并重建世界；移出完成槽后不再引用旧连接状态。
	auto Work = MoveTemp(OverlayNetwork->DecodedEnvelope);
	auto Result = MoveTemp(OverlayNetwork->DecodedEnvelopeResult.GetValue());
	OverlayNetwork->DecodedEnvelopeResult.Reset();
	OverlayNetwork->bEnvelopeDecode = false;
	check(OverlayNetwork->EnvelopeBytes >= Work->GetIngressQuotaBytes());
	OverlayNetwork->EnvelopeBytes -= Work->GetIngressQuotaBytes();
	if (TaskOwnerId != Result.Stamp.OwnerId || TaskOwnerIncarnation != Result.Stamp.OwnerIncarnation ||
		WorldModule->GetWorldEpoch() != Result.Stamp.WorldEpoch || bRejected) return true;
	if (Result.bSuccess && !Result.bCanceled)
	{
		const auto& Message = Work->GetMessage();
		if (bWorldControl)
		{
			if (HandleWelcome(Message)) Result.MarkWorkUsed();
			return true;
		}
		if (Message.Kind != EVoxelMessage::Hello && Message.Kind != EVoxelMessage::Welcome &&
			bWelcomed && Message.Session != Session) Fail(TEXT("Voxel session identity mismatch"));
		else if (Work->IsOverlay())
		{
			if (!bReady) Fail(TEXT("Voxel message arrived before Ready"));
			else HandleOverlayMessage(Message.Kind, Work->TakeOverlay(), Work->IsServer());
		}
		else Handle(Message, Work->IsServer(), Work->IsRepresentationReply() ? &Work->GetRepresentationReply() : nullptr);
		if (!bRejected && TaskOwnerIncarnation == Result.Stamp.OwnerIncarnation &&
			Module.Get() == WorldModule && WorldModule->GetWorldEpoch() == Result.Stamp.WorldEpoch) Result.MarkWorkUsed();
	}
	else if (!Result.bCanceled) Fail(Result.Error.IsEmpty() ? TEXT("Envelope 验证失败") : Result.Error);
	return true;
}

bool UVoxelModuleNetworkComponent::HandleWelcome(const FVoxelWireMessage& InMessage)
{
	UVoxelModule* VoxelModule = Module.Get();
	if (!VoxelModule) return false;
	FVoxelNetworkWelcome Welcome;
	FString Error;
	if (!FVoxelNetworkCodec::DecodeWelcome(InMessage.Payload, Welcome) ||
		Welcome.EchoNonce != ClientNonce || Welcome.Session != InMessage.Session)
	{
		Fail(TEXT("Voxel welcome is invalid"));
		return false;
	}
	FVoxelOverlayHash IncomingManifest;
	FVoxelOverlayHash CurrentManifest;
	if (!FVoxelOverlayRegionCodec::ManifestHash(Welcome.Manifest, IncomingManifest)) { Fail(TEXT("Voxel welcome manifest identity invalid")); return false; }
	const auto* PreviousReplica = VoxelModule->GetOverlayReplica();
	const bool bWorldChanged = VoxelModule->GetRuntime() &&
		(!VoxelModule->IsReady() || !FVoxelOverlayRegionCodec::ManifestHash(VoxelModule->GetManifest(), CurrentManifest) || CurrentManifest != IncomingManifest ||
			Session != Welcome.Session || !PreviousReplica || PreviousReplica->GetAuthorityEpoch() != Welcome.AuthorityWorldEpoch);
	if (bWorldChanged)
	{
		if (!VoxelModule->StopWorld(true, Error)) { Fail(Error); return false; }
	}
	Session = Welcome.Session;
	if (!VoxelModule->IsReady() && !VoxelModule->StartWorld(Welcome.Manifest, true, Error))
	{
		Fail(Error.IsEmpty() ? TEXT("Voxel remote world could not start") : Error);
		return false;
	}
	FVoxelOverlayHash ManifestIdentity;
	if (auto* Replica = VoxelModule->GetOverlayReplica(); Replica &&
		FVoxelOverlayRegionCodec::ManifestHash(Welcome.Manifest, ManifestIdentity))
	{
		Replica->SetAuthority(Welcome.AuthorityWorldEpoch, ManifestIdentity, Welcome.Session);
	}
	FVoxelNetworkReady Ready;
	Ready.RecipeHash = Welcome.Manifest.RecipeHash;
	Ready.RegistryHash = Welcome.Manifest.RegistryHash;
	Ready.BaseSampleHash = Welcome.Manifest.BaseSampleHash;
	TArray<uint8> Payload;
	if (!FVoxelNetworkCodec::EncodeReady(Ready, Payload) ||
		!Send(EVoxelMessage::Ready, Payload, EVoxelTransferPriority::Critical))
	{
		Fail(TEXT("Voxel ready could not be queued"));
		return false;
	}
	bWelcomed = true;
	bReady = true;
	return true;
}

void UVoxelModuleNetworkComponent::Handle(
	const FVoxelWireMessage& InMessage,
	const bool bInServer,
	const FVoxelRepresentationReply* InPreparedReply)
{
	UVoxelModule* VoxelModule = Module.Get();
	if (!VoxelModule)
	{
		return;
	}
	if (bInServer && InMessage.Kind == EVoxelMessage::Hello)
	{
		FVoxelNetworkHello Hello;
		if (!FVoxelNetworkCodec::DecodeHello(InMessage.Payload, Hello) ||
			Hello.ProtocolVersion != VoxelProtocolVersion ||
			!VoxelModule->GetRegistry() ||
			Hello.RegistryHash != VoxelModule->GetRegistry()->Hash ||
			!VoxelModule->IsReady())
		{
			Fail(TEXT("Voxel hello is incompatible with the authoritative world"));
			return;
		}
		Session = VoxelModule->GetSessionId();
		FVoxelNetworkWelcome Welcome;
		Welcome.EchoNonce = Hello.Nonce;
		Welcome.AuthorityWorldEpoch = VoxelModule->GetWorldEpoch();
		Welcome.Session = Session;
		Welcome.Manifest = VoxelModule->GetManifest();
		TArray<uint8> Payload;
		if (!FVoxelNetworkCodec::EncodeWelcome(Welcome, Payload) ||
			!Send(EVoxelMessage::Welcome, Payload, EVoxelTransferPriority::Critical))
		{
			Fail(TEXT("Voxel welcome could not be queued"));
			return;
		}
		bWelcomed = true;
		return;
	}
	if (!bInServer && InMessage.Kind == EVoxelMessage::Welcome)
	{
		HandleWelcome(InMessage);
		return;
	}
	if (bInServer && InMessage.Kind == EVoxelMessage::Ready)
	{
		FVoxelNetworkReady Ready;
		const FVoxelWorldManifest& Manifest = VoxelModule->GetManifest();
		if (!FVoxelNetworkCodec::DecodeReady(InMessage.Payload, Ready) ||
			Ready.RecipeHash != Manifest.RecipeHash ||
			Ready.RegistryHash != Manifest.RegistryHash ||
			Ready.BaseSampleHash != Manifest.BaseSampleHash)
		{
			Fail(TEXT("Voxel ready identity mismatch"));
			return;
		}
		bReady = true;
		return;
	}
	if (!bReady)
	{
		Fail(TEXT("Voxel message arrived before Ready"));
		return;
	}
	if (InMessage.Kind >= EVoxelMessage::SubscribeRegion && InMessage.Kind <= EVoxelMessage::ResyncRequest)
	{
		DecodeOverlayMessage(InMessage, bInServer);
		return;
	}
	if (bInServer && InMessage.Kind == EVoxelMessage::Edit)
	{
		FVoxelEditIntent Intent;
		if (!FVoxelNetworkCodec::DecodeIntent(InMessage.Payload, Intent) ||
			Intent.RequestId <= LastInboundEditId)
		{
			Fail(TEXT("Invalid or replayed voxel edit request"));
			return;
		}
		LastInboundEditId = Intent.RequestId;
		FVoxelEditReply Reply;
		if (bObserverCanBuild && GetAuthorizedObserver())
		{
			Reply = VoxelModule->ExecuteIntent(Controller(), GetAuthorizedObserver(), Intent, false);
		}
		else
		{
			Reply.RequestId = Intent.RequestId;
			Reply.Reason = TEXT("Voxel edits are not authorized for this observer");
		}
		TArray<uint8> Payload;
		if (FVoxelNetworkCodec::EncodeReply(Reply, Payload))
		{
			Send(EVoxelMessage::Reply, Payload, EVoxelTransferPriority::Critical);
		}
		return;
	}
	if (!bInServer && InMessage.Kind == EVoxelMessage::Reply)
	{
		FVoxelEditReply Reply;
		if (FVoxelNetworkCodec::DecodeReply(InMessage.Payload, Reply))
		{
			OnIntentReply.Broadcast(Reply);
		}
		return;
	}
	if (!bInServer && InMessage.Kind == EVoxelMessage::RepresentationReply)
	{
		FVoxelRepresentationReply Reply;
		FString Error;
		if (InPreparedReply) Reply = *InPreparedReply;
		else if (!FVoxelNetworkCodec::DecodeRepresentationReply(InMessage.Payload, Reply, Error))
		{
			Fail(Error.IsEmpty() ? TEXT("Invalid voxel representation reply") : Error);
			return;
		}
		auto* Pending = PendingRepresentationRequests.Find(Reply.Serial);
		if (!Pending && Reply.Serial > 0 && Reply.Serial < NextOutboundId) return; // 已退休请求的重复/晚到回复。
		if (!Pending || Pending->Request.Type != Reply.Type || !(Pending->Request.Key == Reply.Key))
		{
			Fail(TEXT("Voxel representation reply does not match an active request"));
			return;
		}
		if (Pending->bHasReply) return; // 已验证的原回复仍在接纳等待，重复正文不能覆盖它。
		if (Reply.SharedData && (!Reply.SharedData->Lease || Reply.SharedData->Lease->Bytes() <
			VoxelBoundedAllocation::WorkerSharedBytes<FVoxelRepresentationReplyData>() + Reply.SharedData->Bytes.GetAllocatedSize()))
		{ Fail(TEXT("Voxel prepared representation has no valid body reservation")); return; }
		const uint64 Bytes = Reply.SharedData ? Reply.SharedData->Lease->Bytes() : Reply.Data.GetAllocatedSize();
		if (Bytes + PendingRepresentationBytes > 32ull * 1024 * 1024)
		{ Fail(TEXT("Voxel prepared representation capacity exceeded")); return; }
		PendingRepresentationBytes += Bytes;
		const uint64 Serial = Reply.Serial;
		Pending->Reply = MoveTemp(Reply); Pending->bHasReply = true;
		TryApplyRepresentationReply(Serial);
		return;
	}
	if (!bInServer && InMessage.Kind == EVoxelMessage::RepresentationInvalidate)
	{
		FVoxelRepresentationInvalidate Invalidate;
		FString Error;
		if (!FVoxelNetworkCodec::DecodeRepresentationInvalidate(InMessage.Payload, Invalidate, Error))
		{
			Fail(Error.IsEmpty() ? TEXT("Invalid voxel representation invalidation") : Error);
			return;
		}
		VoxelModule->InvalidateRemoteRepresentations(Invalidate);
		OnRepresentationInvalidated.Broadcast(Invalidate);
		return;
	}
	if (bInServer && InMessage.Kind == EVoxelMessage::RepresentationRequest)
	{
		FVoxelRepresentationRequest Request;
		if (!FVoxelNetworkCodec::DecodeRepresentationRequest(InMessage.Payload, Request))
		{
			Fail(TEXT("Invalid voxel representation request"));
			return;
		}
		if (!OverlayNetwork || !ReserveRepresentationTables())
		{
			FVoxelRepresentationReply Retry;
			Retry.Type = Request.Type; Retry.Key = Request.Key; Retry.Serial = Request.Serial;
			Retry.Kind = EVoxelRepresentationReplyKind::Retry;
			TArray<uint8> Payload; FString Error;
			if (FVoxelNetworkCodec::EncodeRepresentationReply(Retry, Payload, Error))
				Send(EVoxelMessage::RepresentationReply, Payload, EVoxelTransferPriority::Bulk);
			return;
		}
		AActor* Observer = GetAuthorizedObserver();
		FVoxelWorldRuntime* Runtime = VoxelModule->GetRuntime();
		if (!Observer || !Runtime || Request.Key.Level > VoxelStreamingGeometry::MaximumSpatialLevel)
		{
			Fail(TEXT("Voxel representation request is outside supported limits"));
			return;
		}
		FIntVector GeometryMin, GeometryMax;
		FString GeometryError;
		if (!Request.Geometry.Validate(GeometryError) || Request.Geometry.GeneratorIdentity != VoxelModule->GetManifest().BaseSampleHash ||
			Request.Geometry.MaterialRevision != VoxelModule->GetManifest().RegistryHash ||
			Request.Geometry.WorldMinZ != VoxelModule->GetManifest().Settings.MinZ ||
			Request.Geometry.WorldMaxZ != VoxelModule->GetManifest().Settings.MaxZ ||
			!Request.Geometry.Key.TryBounds(Request.Geometry.WorldMinZ, Request.Geometry.WorldMaxZ, GeometryMin, GeometryMax))
		{ Fail(TEXT("Voxel representation geometry identity is invalid")); return; }
		const FVoxelGenerationBounds Bounds{GeometryMin, GeometryMax};
		const FVector ObserverCell = Observer->GetActorLocation() / VoxelModule->BlockSize();
		if (!IsRepresentationInRange(ObserverCell, Request.Type, Bounds, VoxelModule->GetViewSettings()))
		{
			Fail(TEXT("Voxel representation request is outside the authorized visual radius"));
			return;
		}
		const auto& Lod = VoxelModule->GetViewSettings().Lod;
		const uint8 MaximumLevel = Request.Type == EVoxelRepresentationWireType::VoxelProxy ? Lod.MaximumVoxelProxyLevel :
			Request.Type == EVoxelRepresentationWireType::SurfaceProxy ? Lod.MaximumSurfaceSpatialLevel : Lod.MaximumMacroSpatialLevel;
		const uint64 Samples = Request.Geometry.Key.SideCells() / Request.Geometry.SampleStepCells;
		const uint64 Cells = Request.Geometry.Key.IsColumn() ? (Samples + 1) * (Samples + 1) : Samples * Samples * Samples;
		const uint64 ShardSide = Request.Geometry.Key.IsColumn() ? 64 : 16;
		const uint64 AxisShards = (Samples + ShardSide - 1) / ShardSide;
		const uint64 ShardCount = Request.Geometry.Key.IsColumn() ? AxisShards * AxisShards : AxisShards * AxisShards * AxisShards;
		if (Request.Key.Level > MaximumLevel || Cells > 1024ull * 1024ull ||
			(!Request.Geometry.Key.IsColumn() && Samples > 128) ||
			ShardCount > 4096)
		{ Fail(TEXT("Voxel representation request exceeds authorized geometry or byte limits")); return; }
		FVoxelCoverageRecord Subscription;
		Subscription.Geometry = Request.Geometry;
		Subscription.Min = GeometryMin;
		Subscription.Max = GeometryMax;
		if (!RepresentationSubscriptions || !RepresentationSubscriptions->Index.Upsert(Subscription, GeometryError))
		{
			FVoxelRepresentationReply Retry;
			Retry.Type = Request.Type; Retry.Key = Request.Key; Retry.Serial = Request.Serial;
			Retry.Kind = EVoxelRepresentationReplyKind::Retry;
			TArray<uint8> Payload;
			if (FVoxelNetworkCodec::EncodeRepresentationReply(Retry, Payload, GeometryError))
				Send(EVoxelMessage::RepresentationReply, Payload, EVoxelTransferPriority::Bulk);
			return;
		}
		RepresentationSubscriptions->MaximumHaloStep = FMath::Max(RepresentationSubscriptions->MaximumHaloStep, Request.Geometry.SampleStepCells);
		const uint64 CurrentRevision = Runtime->GetChangeHierarchy().CaptureVersionSnapshot().GetGeometryRevision(Request.Geometry);
		FVoxelRepresentationReply Reply;
		Reply.Type = Request.Type;
		Reply.Key = Request.Key;
		Reply.Serial = Request.Serial;
		Reply.Revision = CurrentRevision;
		FString Error;
		if (CurrentRevision == 0)
		{
			Reply.Kind = EVoxelRepresentationReplyKind::Unchanged;
			TArray<uint8> Payload;
			if (FVoxelNetworkCodec::EncodeRepresentationReply(Reply, Payload, Error))
			{
				Send(EVoxelMessage::RepresentationReply, Payload, EVoxelTransferPriority::Bulk);
			}
			return;
		}

		FVoxelTaskRequest Task;
		Task.bResumable = true; Task.Kind = EVoxelTaskKind::NetworkRepresentation; Task.WorkClass = EVoxelWorkClass::Exploration;
		Task.Stamp.WorldEpoch = VoxelModule->GetWorldEpoch(); Task.Stamp.OwnerId = TaskOwnerId; Task.Stamp.OwnerIncarnation = TaskOwnerIncarnation;
		check(NextTaskSerial < MAX_uint64); Task.Stamp.Token = ++NextTaskSerial;
		const FVoxelGenerationConsumerKey Consumer{Request.Geometry.Key.IsColumn() ? EVoxelGenerationConsumerKind::Heightfield : EVoxelGenerationConsumerKind::VoxelProxy,
			Task.Stamp.WorldEpoch, Task.Stamp.OwnerId, Task.Stamp.OwnerIncarnation, EVoxelGenerationSpatialKind::Bounds,
			GeometryMin, GeometryMax, Task.Stamp.Token};
		FVoxelRepresentationBuildInput BuildInput;
		if (!FVoxelRepresentationSync::PrepareServerBuild(
			*VoxelModule,
			Request,
			BuildInput,
			Error, &Consumer))
		{
			Reply.Kind = EVoxelRepresentationReplyKind::Retry;
			TArray<uint8> Payload;
			if (FVoxelNetworkCodec::EncodeRepresentationReply(Reply, Payload, Error))
			{
				Send(EVoxelMessage::RepresentationReply, Payload, EVoxelTransferPriority::Bulk);
			}
			return;
		}

		const auto Resources = VoxelModule->GetTerrainResources();
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> MetadataLease;
		const uint64 MetadataBytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelServerRepresentationBuild>() +
			2 * VoxelBoundedAllocation::ArrayBytes<FVoxelTerrainBuildShard>(4096);
		if (!Resources || !Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Input, MetadataBytes, MetadataLease))
		{
			Reply.Kind = EVoxelRepresentationReplyKind::Retry; TArray<uint8> Payload;
			if (FVoxelNetworkCodec::EncodeRepresentationReply(Reply, Payload, Error)) Send(EVoxelMessage::RepresentationReply, Payload, EVoxelTransferPriority::Bulk);
			return;
		}
		const auto PageBuild = VoxelBoundedAllocation::MakeWorkerShared<FVoxelServerRepresentationBuild>(MetadataLease, Resources.ToSharedRef(), Consumer);
		PageBuild->MetadataLease = MoveTemp(MetadataLease);
		if (!PageBuild->Initialize(MoveTemp(BuildInput), Reply, Session, Error)) { Fail(Error); return; }
		FVoxelStreamingSource ServiceSource;
		ServiceSource.Id = TaskOwnerId;
		ServiceSource.Center = FIntVector(FMath::FloorToInt(ObserverCell.X), FMath::FloorToInt(ObserverCell.Y), FMath::FloorToInt(ObserverCell.Z));
		ServiceSource.Direction = Observer->GetActorForwardVector();
		FVoxelTerrainServicePriority ServicePriority;
		if (!FVoxelTerrainPriority::Resolve(Request.Geometry, {}, MakeArrayView(&ServiceSource, 1),
			VoxelModule->GetViewSettings().Fine.InteractiveRadiusCells, ServicePriority, Error))
		{
			Fail(Error);
			return;
		}
		Task.SourcePriority = ServicePriority.SourcePriority;
		Task.DistanceScore = ServicePriority.DistanceCells;
		Task.ForwardScore = ServicePriority.ForwardScore;
		Task.SourceStableKey = ServicePriority.SourceStableKey;
		Task.ServiceBand = ServicePriority.Band;
		Task.Stamp.Revision = CurrentRevision;
		Task.Stamp.bHasGeometryIdentity = true;
		Task.Stamp.GeometryIdentity = Request.Geometry;
		if (Request.Type == EVoxelRepresentationWireType::VoxelProxy)
		{
			Task.Stamp.ViewKey = { Request.Key.Coordinate, Request.Key.Level };
		}
		else if (Request.Type == EVoxelRepresentationWireType::SurfaceProxy)
		{
			Task.Stamp.SurfaceKey = {
				FIntPoint(Request.Key.Coordinate.X, Request.Key.Coordinate.Y),
				Request.Key.Level };
		}
		else
		{
			Task.Stamp.MacroKey = {
				FIntPoint(Request.Key.Coordinate.X, Request.Key.Coordinate.Y),
				Request.Key.Level };
		}
		Task.ReservedBytes = 1;
		Task.InputBytes = PageBuild->MetadataLease->Bytes();
		Task.GetBlockReason = [PageBuild] { return PageBuild->BlockReason(); };
		Task.Execute = [PageBuild](const TAtomic<bool>& InCancel) { return PageBuild->Advance(InCancel); };
		const TWeakObjectPtr<UVoxelModuleNetworkComponent> WeakThis(this);
		const FGuid RequestSession = Session;
		Task.Apply = [WeakThis, RequestSession, Reply, PageBuild](FVoxelTaskResult&& InResult) mutable
		{
			UVoxelModuleNetworkComponent* Component = WeakThis.Get();
			if (!Component || Component->Session != RequestSession || !Component->bReady ||
				Component->TaskOwnerId != InResult.Stamp.OwnerId || Component->TaskOwnerIncarnation != InResult.Stamp.OwnerIncarnation)
			{
				return;
			}
			const UVoxelModule* CurrentModule = Component->Module.Get();
			const bool bCurrent = CurrentModule && CurrentModule->GetRuntime() && CurrentModule->GetWorldEpoch() == InResult.Stamp.WorldEpoch &&
				CurrentModule->IsOverlayReadCurrent(PageBuild->Page.GetOverlayReadResult()) &&
				CurrentModule->GetRuntime()->GetChangeHierarchy().CaptureVersionSnapshot().GetGeometryRevision(InResult.Stamp.GeometryIdentity) == Reply.Revision;
			if (InResult.bSuccess && !InResult.bCanceled && bCurrent && InResult.CustomPayload)
			{
				const auto Prepared = StaticCastSharedPtr<const FVoxelServerRepresentationResult>(InResult.CustomPayload);
				// 当前 Apply 唯一接管正文，不复制大数组、不重复计算 CRC。
				auto Mutable = ConstCastSharedPtr<FVoxelServerRepresentationResult>(Prepared);
				if (Component->Transfer.EnqueuePrepared(EVoxelTransferPriority::Bulk, MoveTemp(Mutable->Envelope), Mutable->Crc, Mutable->Lease))
					InResult.MarkWorkUsed();
				return;
			}
			Reply.Kind = InResult.bCanceled ? EVoxelRepresentationReplyKind::Retry :
				!InResult.bSuccess ? EVoxelRepresentationReplyKind::Rejected :
				EVoxelRepresentationReplyKind::Retry;
			Reply.Data.Reset();
			TArray<uint8> Payload;
			FString ReplyError;
			if (FVoxelNetworkCodec::EncodeRepresentationReply(Reply, Payload, ReplyError))
			{
				Component->Send(
					EVoxelMessage::RepresentationReply,
					Payload,
					EVoxelTransferPriority::Bulk);
			}
		};
		if (!VoxelModule->EnqueueNetworkRepresentationTask(MoveTemp(Task)))
		{
			Reply.Kind = EVoxelRepresentationReplyKind::Retry;
			Reply.Data.Reset();
			TArray<uint8> Payload;
			if (FVoxelNetworkCodec::EncodeRepresentationReply(Reply, Payload, Error))
			{
				Send(EVoxelMessage::RepresentationReply, Payload, EVoxelTransferPriority::Bulk);
			}
		}
		return;
	}
}

void UVoxelModuleNetworkComponent::RefreshInterest(const double InNow)
{
	auto* VoxelModule = Module.Get();
	auto* Observer = GetAuthorizedObserver();
	if (!VoxelModule || !Observer || !VoxelModule->IsReady()) return;
	FVoxelStreamingSource Source;
	const FVector Position = Observer->GetActorLocation() / VoxelModule->BlockSize();
	Source.Center = FIntVector(FMath::FloorToInt(Position.X), FMath::FloorToInt(Position.Y), FMath::FloorToInt(Position.Z));
	Source.Direction = Observer->GetActorForwardVector();
	Source.VelocityCellsPerSecond = Observer->GetVelocity() / VoxelModule->BlockSize();
	Source.Capabilities = EVoxelStreamingCapability::Data | EVoxelStreamingCapability::Collision | EVoxelStreamingCapability::Simulation;
	Source.Normalize();
	if (SourceId.IsValid()) VoxelModule->UpdateSource(SourceId, Source);
	else SourceId = VoxelModule->RegisterSource(this, Source);
	LastInterest = InNow;
}
void UVoxelModuleNetworkComponent::OnCommit(const FVoxelEditBatch& InBatch)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !bReady) return;
	auto* WorldModule = Module.Get();
	if (!WorldModule || !WorldModule->GetRuntime()) return;
	if (RepresentationSubscriptions)
		RepresentationSubscriptions->LatestCut = WorldModule->GetRuntime()->GetChangeHierarchy().CaptureVersionSnapshot();
	CaptureOverlayCommit(InBatch);
}

APlayerController* UVoxelModuleNetworkComponent::Controller() const
{
	return Cast<APlayerController>(GetOwner());
}

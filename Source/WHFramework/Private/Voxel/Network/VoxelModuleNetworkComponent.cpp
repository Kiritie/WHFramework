#include "Voxel/Network/VoxelModuleNetworkComponent.h"
#include "VoxelModuleNetworkOverlayState.h"
#include "VoxelServerRepresentationBuild.h"
#include "VoxelRepresentationSubscriptionState.h"
#include "Voxel/Rendering/VoxelViewLod.h"

#include "GameFramework/PlayerController.h"
#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Generation/VoxelGenerationAllocation.h"
#include "Voxel/Network/VoxelGeometryWire.h"
#include "Voxel/Network/VoxelNetworkCodec.h"
#include "Voxel/Generation/VoxelManifestCodec.h"
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

UVoxelModuleNetworkComponent::UVoxelModuleNetworkComponent(FVTableHelper& Helper)
	: Super(Helper)
{
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
	Transfer.Tick(Now, [this](TArray<uint8>&& InFragment)
	{
		FVoxelRPCPacket Packet;
		Packet.Bytes = MoveTemp(InFragment);
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
	else if (!bWelcomed && !bPendingReady && Now - LastHello >= 1.0)
	{
		SendHello(Now);
	}
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
	NetworkStateConsumer = {};
	NetworkTransferConsumer = {};
	NetworkStateCapacityConsumer = {};
	NetworkStateCapacityVersion = 0;
	bNetworkStateWaitingCapacity = false;
	bNetworkTransferBound = false;
	PendingWelcomeConsumer = {};
	PendingWelcomeSession.Invalidate();
	PendingWelcomeNonce = 0;
	PendingWelcomeCapacityVersion = 0;
	HelloCapacityVersion = 0;
	bPendingWelcome = false;
	bPendingWelcomeWaitingCapacity = false;
	bHelloWaitingCapacity = false;
	PendingReadyConsumer = {};
	PendingReadySession.Invalidate();
	PendingReadyCapacityVersion = 0;
	bPendingReady = false;
	bPendingReadyServer = false;
	bPendingReadyWaitingCapacity = false;
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
	if (bPendingWelcome) return FinishPendingWelcome();
	if (bPendingReady) return FinishPendingReady();
	return ApplyDecodedEnvelope(true);
}

bool UVoxelModuleNetworkComponent::FinishPendingReady()
{
	auto* WorldModule = Module.Get();
	if (!bPendingReady || !WorldModule || bRejected) return false;
	if (!PendingReadyConsumer.IsValid() || PendingReadyConsumer.WorldEpoch != WorldModule->GetWorldEpoch() ||
		PendingReadyConsumer.OwnerId != TaskOwnerId || PendingReadyConsumer.OwnerIncarnation != TaskOwnerIncarnation ||
		PendingReadySession != Session)
	{
		Fail(TEXT("InvalidResource: original Ready control cut changed"));
		return false;
	}
	FString Error;
	if (!EnsureNetworkState(Error))
	{
		if (!Error.StartsWith(TEXT("CapacityBlocked"))) Fail(Error);
		else WorldModule->QueueTerrainWorldControl(this);
		return false;
	}
	if (bPendingReadyServer)
	{
		bPendingReady = false;
		bReady = true;
		return true;
	}
	const auto Resources = OverlayNetwork->Resources;
	uint64 BeforeAdmission = 0;
	if (!Resources->TryGetReleaseVersionExcludingOwner(PendingReadyConsumer, BeforeAdmission) ||
		(bPendingReadyWaitingCapacity && BeforeAdmission <= PendingReadyCapacityVersion))
	{
		WorldModule->QueueTerrainWorldControl(this);
		return false;
	}
	bPendingReadyWaitingCapacity = false;
	const auto& Manifest = WorldModule->GetManifest();
	const uint64 Values[] = {Manifest.RecipeHash, Manifest.RegistryHash, Manifest.BaseSampleHash};
	uint8 Payload[sizeof(Values)];
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Values); ++Index)
	{
		if (Values[Index] == 0)
		{
			Fail(TEXT("InvalidResource: original Ready manifest identity"));
			return false;
		}
		for (int32 Byte = 0; Byte < sizeof(uint64); ++Byte)
			Payload[Index * sizeof(uint64) + Byte] = uint8(Values[Index] >> (Byte * 8));
	}
	FVoxelGenerationRequiredSourceScope StateSource(Resources.Get(), OverlayNetwork->ResourceLease.Get());
	if (!Send(EVoxelMessage::Ready, MakeArrayView(Payload), EVoxelTransferPriority::Critical, &PendingReadyConsumer, &Error))
	{
		if (!Error.StartsWith(TEXT("CapacityBlocked")) && !Error.StartsWith(TEXT("PlanningPending")))
		{
			Fail(Error);
			return false;
		}
		bPendingReadyWaitingCapacity = Error.StartsWith(TEXT("CapacityBlocked"));
		PendingReadyCapacityVersion = BeforeAdmission;
		WorldModule->QueueTerrainWorldControl(this);
		return false;
	}
	bPendingReady = false;
	bWelcomed = true;
	bReady = true;
	return true;
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

void UVoxelModuleNetworkComponent::SendHello(const double InNow)
{
	auto* WorldModule = Module.Get();
	if (!WorldModule || bRejected || bWelcomed || bPendingReady) return;
	if (ClientNonce == 0) ClientNonce = FPlatformTime::Cycles64() | 1;
	FString Error;
	if (!EnsureTransferResources(Error))
	{
		if (!Error.StartsWith(TEXT("CapacityBlocked"))) Fail(Error);
		return;
	}
	const auto Resources = Transfer.GetResources();
	uint64 BeforeAdmission = 0;
	if (!Resources->TryGetReleaseVersionExcludingOwner(NetworkTransferConsumer, BeforeAdmission) ||
		(bHelloWaitingCapacity && BeforeAdmission <= HelloCapacityVersion)) return;
	bHelloWaitingCapacity = false;
	const uint64 RegistryHash = WorldModule->GetRegistry() ? WorldModule->GetRegistry()->Hash : 0;
	if (RegistryHash == 0) return;
	uint8 Payload[20];
	for (int32 Byte = 0; Byte < 4; ++Byte) Payload[Byte] = uint8(VoxelProtocolVersion >> (Byte * 8));
	for (int32 Byte = 0; Byte < 8; ++Byte)
	{
		Payload[4 + Byte] = uint8(ClientNonce >> (Byte * 8));
		Payload[12 + Byte] = uint8(RegistryHash >> (Byte * 8));
	}
	if (!Send(EVoxelMessage::Hello, MakeArrayView(Payload), EVoxelTransferPriority::Critical, &NetworkTransferConsumer, &Error))
	{
		bHelloWaitingCapacity = Error.StartsWith(TEXT("CapacityBlocked"));
		HelloCapacityVersion = BeforeAdmission;
		if (!bHelloWaitingCapacity && !Error.StartsWith(TEXT("PlanningPending"))) Fail(Error);
		return;
	}
	LastHello = InNow;
}

bool UVoxelModuleNetworkComponent::Send(const EVoxelMessage InKind,
	const TConstArrayView<uint8> InPayload, const EVoxelTransferPriority InPriority,
	const FVoxelGenerationConsumerKey* InConsumer, FString* OutError)
{
	if (InPayload.Num() > FVoxelNetworkCodec::MaxWireBytes - 64)
	{
		if (OutError) *OutError = TEXT("Oversized: voxel envelope wire length");
		return false;
	}
	FString Error;
	if (!EnsureTransferResources(Error))
	{
		if (OutError) *OutError = MoveTemp(Error);
		return false;
	}
	const auto Resources = Transfer.GetResources();
	FVoxelGenerationConsumerKey Consumer;
	if (InConsumer) Consumer = *InConsumer;
	else
	{
		if (NextTaskSerial == MAX_uint64)
		{
			if (OutError) *OutError = TEXT("Oversized: voxel send identity");
			return false;
		}
		Consumer = FVoxelGenerationConsumerKey::Section(Resources->GetControlConsumer().WorldEpoch, TaskOwnerId,
			TaskOwnerIncarnation, {}, ++NextTaskSerial);
	}
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	TArray<uint8> Message;
	FVoxelGenerationRequiredSourceScope TransferSource(Resources.Get(), Transfer.GetMetadataLease().Get());
	FVoxelGenerationRequiredCapacity Required;
	if (!FVoxelGenerationRequiredSourceScope::Collect(*Resources, nullptr, Required, Error))
	{
		if (OutError) *OutError = MoveTemp(Error);
		return false;
	}
	const uint64 Bytes = VoxelGenerationAllocation::Private::ArrayReserveBytes<uint8, FDefaultAllocator>(InPayload.Num() + 29);
	const auto Admission = Resources->TryAcquire(Consumer, EVoxelTerrainResourcePool::Staged, Bytes, Lease, 0,
		Required.Bytes[int32(EVoxelTerrainResourcePool::Input)], Required.LeaseCount);
	if (Admission != EVoxelTerrainResourceAdmission::Granted)
	{
		if (OutError) *OutError = Admission == EVoxelTerrainResourceAdmission::CapacityBlocked ?
			TEXT("CapacityBlocked: original voxel envelope send") : Admission == EVoxelTerrainResourceAdmission::InvalidRequest ?
			TEXT("InvalidResource: original voxel envelope consumer") : TEXT("Oversized: original voxel envelope send");
		return false;
	}
	const FGuid EnvelopeSession = InKind == EVoxelMessage::Hello ? FGuid() : Session;
	if (!FVoxelNetworkCodec::Encode(InKind, EnvelopeSession, InPayload, Message) ||
		VoxelGenerationAllocation::AllocatedBytes(Message) != Lease->Bytes())
	{
		if (OutError) *OutError = TEXT("InvalidResource: voxel envelope reserve contract");
		return false;
	}
	if (!Transfer.Enqueue(InPriority, MoveTemp(Message), MoveTemp(Lease), &Error))
	{
		if (OutError) *OutError = MoveTemp(Error);
		return false;
	}
	if (OutError) OutError->Reset();
	return true;
}

void UVoxelModuleNetworkComponent::ProcessBootstrapPacket(const FVoxelRPCPacket& InPacket, const bool bInServer)
{
	FVoxelByteReader Fragment(InPacket.Bytes);
	const uint32 Magic = Fragment.U32();
	const uint64 Id = Fragment.U64();
	const uint16 Index = Fragment.U16();
	const uint16 Count = Fragment.U16();
	const uint32 Total = Fragment.U32();
	const uint32 Crc = Fragment.U32();
	const auto Bytes = Fragment.BlobView(4096);
	if (Magic != 0x34475246 || Id == 0 || Index != 0 || Count != 1 || Total != uint32(Bytes.Num()) ||
		!Fragment.End() || Total > uint32(FVoxelManifestCodec::MaxBytes + 64 + 29) ||
		FCrc::MemCrc32(Bytes.GetData(), Bytes.Num()) != Crc)
	{
		Fail(TEXT("InvalidResource: malformed original bootstrap fragment"));
		return;
	}
	FVoxelByteReader Envelope(Bytes);
	const uint32 WireMagic = Envelope.U32();
	const uint32 Protocol = Envelope.U32();
	const auto Kind = EVoxelMessage(Envelope.U8());
	const FGuid InSession = Envelope.Guid();
	const auto Payload = Envelope.BlobView(FVoxelManifestCodec::MaxBytes + 64);
	const bool bHello = bInServer && Kind == EVoxelMessage::Hello && !InSession.IsValid();
	const bool bReadyControl = bInServer && Kind == EVoxelMessage::Ready && bWelcomed && InSession == Session;
	const bool bWelcome = !bInServer && Kind == EVoxelMessage::Welcome && InSession.IsValid();
	if (WireMagic != 0x34544E56 || Protocol != VoxelProtocolVersion || !Envelope.End() ||
		(!bHello && !bReadyControl && !bWelcome))
	{
		Fail(TEXT("InvalidResource: original bootstrap envelope identity"));
		return;
	}
	if (bReadyControl) HandleReadyPayload(Payload);
	else if (bInServer) HandleHelloPayload(Payload);
	else HandleWelcomePayload(Payload, InSession);
}

void UVoxelModuleNetworkComponent::DeferIngressResync(const FVoxelGenerationConsumerKey& InConsumer,
	const uint64 InBeforeAdmission, const bool bInServer, const bool bInAwaitRelease)
{
	const auto StateOwner = OverlayNetwork;
	auto* WorldModule = Module.Get();
	if (!StateOwner || !WorldModule || !WorldModule->IsReady() || !InConsumer.IsValid() ||
		InConsumer.WorldEpoch != WorldModule->GetWorldEpoch() || InConsumer.OwnerId != TaskOwnerId ||
		InConsumer.OwnerIncarnation != TaskOwnerIncarnation ||
		(StateOwner->Resources->GetControlConsumer().IsValid() &&
			StateOwner->Resources->GetControlConsumer().WorldEpoch != InConsumer.WorldEpoch)) return;
	StateOwner->IngressCapacityConsumer = InConsumer;
	StateOwner->IngressCapacityVersion = InBeforeAdmission;
	StateOwner->bIngressWaitingCapacity = bInAwaitRelease;
	if (!StateOwner->bIngressResyncPending)
	{
		StateOwner->bIngressResyncPending = true;
		UE_LOG(LogTemp, Warning, TEXT("Voxel transport gap; original subscription requires resynchronization"));
	}
	if (bInServer)
	{
		StateOwner->bNeedsSnapshot |= !StateOwner->Regions.IsEmpty();
		return;
	}
	auto* Replica = WorldModule->GetOverlayReplica();
	if (Replica)
	{
		for (const auto& Key : StateOwner->Regions)
		{
			if (Replica->GetState(Key) != EVoxelOverlayReplicaState::Unknown) Replica->MarkGap(Key);
		}
	}
	StateOwner->RetryCount = FMath::Max(StateOwner->RetryCount, 3);
	StateOwner->LastRetry = -10.0;
}

bool UVoxelModuleNetworkComponent::HandleHelloPayload(const TConstArrayView<uint8> InPayload)
{
	auto* WorldModule = Module.Get();
	FVoxelNetworkHello Hello;
	if (!WorldModule || !FVoxelNetworkCodec::DecodeHello(InPayload, Hello) ||
		!WorldModule->GetRegistry() || Hello.RegistryHash != WorldModule->GetRegistry()->Hash || !WorldModule->IsReady())
	{
		Fail(TEXT("Voxel hello is incompatible with the authoritative world"));
		return false;
	}
	if (Hello.Nonce == PendingWelcomeNonce && bWelcomed && !bPendingWelcome &&
		PendingWelcomeConsumer.WorldEpoch == WorldModule->GetWorldEpoch() &&
		PendingWelcomeConsumer.OwnerId == TaskOwnerId && PendingWelcomeConsumer.OwnerIncarnation == TaskOwnerIncarnation &&
		PendingWelcomeSession == WorldModule->GetSessionId()) return true;
	if (!bPendingWelcome || PendingWelcomeNonce != Hello.Nonce)
	{
		if (NextTaskSerial == MAX_uint64)
		{
			Fail(TEXT("Oversized: original Welcome identity"));
			return false;
		}
		PendingWelcomeConsumer = FVoxelGenerationConsumerKey::Section(WorldModule->GetWorldEpoch(), TaskOwnerId,
			TaskOwnerIncarnation, {}, ++NextTaskSerial);
		PendingWelcomeNonce = Hello.Nonce;
		PendingWelcomeSession = WorldModule->GetSessionId();
		bPendingWelcomeWaitingCapacity = false;
		bPendingWelcome = true;
	}
	Session = PendingWelcomeSession;
	if (!WorldModule->GetTerrainFrameContext().IsActive()) FinishPendingWelcome();
	else WorldModule->QueueTerrainWorldControl(this);
	return !bRejected;
}

bool UVoxelModuleNetworkComponent::FinishPendingWelcome()
{
	auto* WorldModule = Module.Get();
	if (!WorldModule || !bPendingWelcome || bRejected) return false;
	if (WorldModule->GetTerrainFrameContext().IsActive())
	{
		WorldModule->QueueTerrainWorldControl(this);
		return false;
	}
	if (!WorldModule->IsReady() || PendingWelcomeConsumer.WorldEpoch != WorldModule->GetWorldEpoch() ||
		PendingWelcomeConsumer.OwnerId != TaskOwnerId || PendingWelcomeConsumer.OwnerIncarnation != TaskOwnerIncarnation ||
		PendingWelcomeSession != WorldModule->GetSessionId() || PendingWelcomeSession != Session)
	{
		Fail(TEXT("InvalidResource: original Welcome control cut changed"));
		return false;
	}
	FString Error;
	if (!EnsureNetworkState(Error))
	{
		if (Error.StartsWith(TEXT("CapacityBlocked"))) WorldModule->QueueTerrainWorldControl(this);
		else Fail(Error);
		return false;
	}
	const auto Resources = Transfer.GetResources();
	uint64 BeforeAdmission = 0;
	if (!Resources->TryGetReleaseVersionExcludingOwner(PendingWelcomeConsumer, BeforeAdmission) ||
		(bPendingWelcomeWaitingCapacity && BeforeAdmission <= PendingWelcomeCapacityVersion))
	{
		WorldModule->QueueTerrainWorldControl(this);
		return false;
	}
	bPendingWelcomeWaitingCapacity = false;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Scratch;
	TArray<uint8> Payload;
	FVoxelGenerationRequiredSourceScope StateSource(Resources.Get(), OverlayNetwork->ResourceLease.Get());
	FVoxelGenerationRequiredSourceScope DirectorySource(Resources.Get(), Transfer.GetMetadataLease().Get());
	FVoxelGenerationRequiredCapacity Required;
	if (!FVoxelGenerationRequiredSourceScope::Collect(*Resources, nullptr, Required, Error))
	{
		Fail(Error);
		return false;
	}
	const uint64 Bytes = VoxelGenerationAllocation::Private::ArrayReserveBytes<uint8, FDefaultAllocator>(FVoxelManifestCodec::MaxBytes) +
		VoxelGenerationAllocation::Private::ArrayReserveBytes<uint8, FDefaultAllocator>(FVoxelManifestCodec::MaxBytes + 64);
	const auto Admission = Resources->TryAcquire(PendingWelcomeConsumer, EVoxelTerrainResourcePool::Working, Bytes, Scratch, 0,
		Required.Bytes[int32(EVoxelTerrainResourcePool::Input)], Required.LeaseCount);
	if (Admission != EVoxelTerrainResourceAdmission::Granted)
	{
		bPendingWelcomeWaitingCapacity = Admission == EVoxelTerrainResourceAdmission::CapacityBlocked;
		PendingWelcomeCapacityVersion = BeforeAdmission;
		if (bPendingWelcomeWaitingCapacity) WorldModule->QueueTerrainWorldControl(this);
		else Fail(Admission == EVoxelTerrainResourceAdmission::CapacityUnachievable ?
			TEXT("Oversized: original Welcome native buffers") : TEXT("InvalidResource: original Welcome consumer"));
		return false;
	}
	FVoxelNetworkWelcome Welcome;
	Welcome.EchoNonce = PendingWelcomeNonce;
	Welcome.AuthorityWorldEpoch = WorldModule->GetWorldEpoch();
	Welcome.Session = Session;
	Welcome.Manifest = WorldModule->GetManifest();
	FVoxelGenerationRequiredSourceScope ScratchSource(Resources.Get(), Scratch.Get());
	if (!FVoxelNetworkCodec::EncodeWelcome(Welcome, Payload))
	{
		Fail(TEXT("InvalidResource: original Welcome encoding"));
		return false;
	}
	if (!Send(EVoxelMessage::Welcome, Payload, EVoxelTransferPriority::Critical, &PendingWelcomeConsumer, &Error))
	{
		bPendingWelcomeWaitingCapacity = Error.StartsWith(TEXT("CapacityBlocked"));
		PendingWelcomeCapacityVersion = BeforeAdmission;
		if (bPendingWelcomeWaitingCapacity || Error.StartsWith(TEXT("PlanningPending"))) WorldModule->QueueTerrainWorldControl(this);
		else Fail(Error);
		return false;
	}
	bPendingWelcome = false;
	bWelcomed = true;
	return true;
}

void UVoxelModuleNetworkComponent::Receive(const FVoxelRPCPacket& InPacket, const bool bInServer)
{
	if (bRejected) return;
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
	if ((!OverlayNetwork && (!bReady || !bNetworkTransferBound)) || (bInServer && !bReady))
	{
		ProcessBootstrapPacket(InPacket, bInServer);
		return;
	}
	if (!OverlayNetwork || !Module.IsValid() || !Module->IsReady() || !bNetworkTransferBound)
	{
		Fail(TEXT("InvalidResource: original network packet world is unavailable"));
		return;
	}
	if (InPacket.Bytes.Num() > 4608 || NextTaskSerial == MAX_uint64)
	{
		Fail(TEXT("Oversized: original network packet length or identity"));
		return;
	}
	const uint64 Bytes = VoxelGenerationAllocation::Private::ArrayReserveBytes<uint8, FDefaultAllocator>(InPacket.Bytes.Num());
	const uint64 NodeBytes = TVoxelNetworkQueue<FVoxelOverlayQueuedPacket>::NodeBytes();
	if (InPacket.Bytes.IsEmpty())
	{
		Fail(TEXT("InvalidResource: original network packet is empty"));
		return;
	}
	const auto StateOwner = OverlayNetwork;
	const auto Resources = StateOwner->Resources;
	if (StateOwner->bIngressWaitingCapacity)
	{
		uint64 Current = 0;
		if (!Resources->TryGetReleaseVersionExcludingOwner(StateOwner->IngressCapacityConsumer, Current) ||
			Current <= StateOwner->IngressCapacityVersion) return;
		StateOwner->bIngressWaitingCapacity = false;
	}
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	FVoxelOverlayQueuedPacket Queued;
	Queued.Consumer = FVoxelGenerationConsumerKey::Section(Module->GetWorldEpoch(), TaskOwnerId,
		TaskOwnerIncarnation, {}, ++NextTaskSerial);
	const uint64 BeforeAdmission = Resources->ReleaseVersionExcludingOwner(Queued.Consumer);
	if (StateOwner->PacketBytes + Bytes > 32ull * 1024 * 1024)
	{
		DeferIngressResync(Queued.Consumer, BeforeAdmission, bInServer);
		return;
	}
	const auto DirectoryLease = Transfer.GetMetadataLease();
	FVoxelGenerationRequiredSourceScope StateSource(Resources.Get(), StateOwner->ResourceLease.Get());
	FVoxelGenerationRequiredSourceScope DirectorySource(Resources.Get(), DirectoryLease.Get());
	FVoxelGenerationRequiredCapacity Required;
	FString Error;
	if (!FVoxelGenerationRequiredSourceScope::Collect(*Resources, nullptr, Required, Error)) { Fail(Error); return; }
	const auto Admission = Resources->TryAcquire(Queued.Consumer, EVoxelTerrainResourcePool::Input, Bytes + NodeBytes, Lease, 1,
		Required.Bytes[int32(EVoxelTerrainResourcePool::Input)], Required.LeaseCount);
	if (Admission != EVoxelTerrainResourceAdmission::Granted)
	{
		if (Admission == EVoxelTerrainResourceAdmission::CapacityBlocked)
		{
			DeferIngressResync(Queued.Consumer, BeforeAdmission, bInServer);
			return;
		}
		DeferIngressResync(Queued.Consumer, BeforeAdmission, bInServer);
		Fail(Admission == EVoxelTerrainResourceAdmission::CapacityUnachievable ? TEXT("Oversized: original RPC packet ingress") :
			TEXT("InvalidResource: original RPC packet consumer"));
		return;
	}
	auto NodeLease = Lease->Split(NodeBytes);
	if (!NodeLease) { Fail(TEXT("InvalidResource: packet queue prepaid node")); return; }
	Queued.bServer = bInServer;
	Queued.ResourceLease = MoveTemp(Lease);
	Queued.Packet.Bytes.Reserve(InPacket.Bytes.Num());
	Queued.Packet.Bytes.Append(InPacket.Bytes);
	const auto BodyLease = Queued.ResourceLease;
	StateOwner->PacketBytes += Bytes;
	if (!StateOwner->Packets.Enqueue(MoveTemp(Queued), MoveTemp(NodeLease)))
	{
		Fail(TEXT("InvalidResource: packet queue node grant"));
		return;
	}
	BodyLease->MarkPublished();
}

bool UVoxelModuleNetworkComponent::ProcessPacket(const FVoxelOverlayQueuedPacket& InPacket)
{
	const auto StateOwner = OverlayNetwork;
	auto* WorldModule = Module.Get();
	if (!StateOwner || !WorldModule || !WorldModule->IsReady() || !bNetworkTransferBound ||
		InPacket.Consumer.WorldEpoch != WorldModule->GetWorldEpoch() || InPacket.Consumer.OwnerId != TaskOwnerId ||
		InPacket.Consumer.OwnerIncarnation != TaskOwnerIncarnation)
	{
		Fail(TEXT("InvalidResource: original packet head identity changed"));
		return true;
	}
	const auto Resources = StateOwner->Resources;
	uint64 BeforeAdmission = 0;
	if (!Resources->TryGetReleaseVersionExcludingOwner(InPacket.Consumer, BeforeAdmission) ||
		(StateOwner->bPacketHeadWaitingCapacity && BeforeAdmission <= StateOwner->PacketHeadCapacityVersion)) return false;
	StateOwner->bPacketHeadWaitingCapacity = false;
	const auto DirectoryLease = Transfer.GetMetadataLease();
	FVoxelGenerationRequiredSourceScope PacketSource(Resources.Get(), InPacket.ResourceLease.Get());
	FVoxelGenerationRequiredSourceScope StateSource(Resources.Get(), StateOwner->ResourceLease.Get());
	FVoxelGenerationRequiredSourceScope DirectorySource(Resources.Get(), DirectoryLease.Get());
	FVoxelGenerationRequiredCapacity Required;
	FString Error;
	if (!FVoxelGenerationRequiredSourceScope::Collect(*Resources, nullptr, Required, Error)) { Fail(Error); return true; }
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Metadata;
	const uint64 NodeBytes = TVoxelNetworkQueue<TSharedPtr<FVoxelNetworkEnvelopeDecode, ESPMode::ThreadSafe>>::NodeBytes();
	const uint64 MetadataBytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelNetworkEnvelopeDecode>();
	const auto Admission = Resources->TryAcquire(InPacket.Consumer, EVoxelTerrainResourcePool::Input, MetadataBytes + NodeBytes,
		Metadata, 1, Required.Bytes[int32(EVoxelTerrainResourcePool::Input)], Required.LeaseCount);
	if (Admission != EVoxelTerrainResourceAdmission::Granted)
	{
		StateOwner->bPacketHeadWaitingCapacity = Admission == EVoxelTerrainResourceAdmission::CapacityBlocked;
		StateOwner->PacketHeadCapacityVersion = BeforeAdmission;
		if (!StateOwner->bPacketHeadWaitingCapacity) Fail(Admission == EVoxelTerrainResourceAdmission::CapacityUnachievable ?
			TEXT("Oversized: original envelope ingress metadata") : TEXT("InvalidResource: original envelope ingress consumer"));
		return !StateOwner->bPacketHeadWaitingCapacity;
	}
	auto NodeLease = Metadata->Split(NodeBytes);
	if (!NodeLease) { Fail(TEXT("InvalidResource: envelope queue prepaid node")); return true; }
	FVoxelGenerationRequiredSourceScope MetadataSource(Resources.Get(), Metadata.Get());
	FVoxelGenerationRequiredSourceScope NodeSource(Resources.Get(), NodeLease.Get());
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> CompletedLease;
	TArray<uint8> Completed;
	uint32 Crc = 0;
	if (!Transfer.Assemble(InPacket.Packet.Bytes, FPlatformTime::Seconds(), Completed, Crc, &CompletedLease, &Error))
	{
		StateOwner->bPacketHeadWaitingCapacity = Error.StartsWith(TEXT("CapacityBlocked"));
		StateOwner->PacketHeadCapacityVersion = BeforeAdmission;
		if (StateOwner->bPacketHeadWaitingCapacity || Error.StartsWith(TEXT("PlanningPending"))) return false;
		Fail(Error);
		return true;
	}
	if (Completed.IsEmpty()) return true;
	auto Work = FVoxelNetworkEnvelopeDecode::Create(Resources.ToSharedRef(), InPacket.Consumer,
		MoveTemp(Completed), MoveTemp(CompletedLease), Crc, InPacket.bServer, Error, MoveTemp(Metadata));
	if (!Work) { Fail(Error); return true; }
	const uint64 Quota = Work->GetIngressQuotaBytes();
	if (Quota + PendingRepresentationBytes + StateOwner->EnvelopeBytes + StateOwner->PacketBytes + StateOwner->InboundBytes > 32ull * 1024 * 1024)
	{
		Fail(TEXT("完整 Envelope 超过连接接收配额"));
		return true;
	}
	StateOwner->EnvelopeBytes += Quota;
	const auto AcceptedWork = Work;
	if (!StateOwner->Envelopes.Enqueue(MoveTemp(Work), MoveTemp(NodeLease)))
	{
		Fail(TEXT("InvalidResource: envelope queue node grant"));
		return true;
	}
	AcceptedWork->VisitResourceLeases([](const TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& InLease)
	{
		if (InLease) InLease->MarkPublished();
	});
	return true;
}

void UVoxelModuleNetworkComponent::AdvanceEnvelopeDecode(const double InNow)
{
	auto* WorldModule = Module.Get();
	if (!OverlayNetwork || !WorldModule || OverlayNetwork->bEnvelopeDecode) return;
	const auto StateOwner = OverlayNetwork;
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
	auto& Admission = OverlayNetwork->EnvelopeAdmission;
	if (!Admission.Request.IsSet()) Admission.Request.Emplace();
	auto& Task = Admission.Request.GetValue();
	Task.Kind = EVoxelTaskKind::NetworkRepresentation;
	Task.Stamp = OverlayNetwork->EnvelopeHeadStamp;
	Task.WorkClass = EVoxelWorkClass::Visible;
	Task.ServiceBand = EVoxelTerrainServiceBand::P2;
	Task.SourceStableKey = TaskOwnerId;
	Task.FirstRunnableSeconds = OverlayNetwork->EnvelopeHeadFirstQueuedSeconds;
	Task.InputBytes = Work->GetResourceAccountedBytes();
	Task.ReservedBytes = 1;
	Task.bResumable = true;
	auto GetBlockReason = [Work] { return Work->GetBlockReason(); };
	auto Execute = [Work](const TAtomic<bool>& Cancel) { return Work->Advance(Cancel); };
	auto Apply = [Weak = TWeakObjectPtr<UVoxelModuleNetworkComponent>(this), Work](FVoxelTaskResult&& Result)
	{
		if (auto* Component = Weak.Get()) Component->StageDecodedEnvelope(Work, MoveTemp(Result));
	};
	FString BindingError;
	const auto BindingResources = WorldModule->GetTerrainResources();
	if (!BindingResources) return;
	if (!Admission.Bind(BindingResources.ToSharedRef(), WorldModule->GetTerrainTaskControlConsumer(),
		MoveTemp(GetBlockReason), MoveTemp(Execute), MoveTemp(Apply), BindingError))
	{
		if (!BindingError.StartsWith(TEXT("CapacityBlocked"))) Fail(BindingError);
		return;
	}
	if (Admission.Enqueue([WorldModule](FVoxelTaskRequest&& Request)
		{ return WorldModule->EnqueueNetworkRepresentationTask(MoveTemp(Request)); }, BindingResources.ToSharedRef(),
		WorldModule->GetTerrainTaskControlConsumer(), BindingError))
	{
		if (OverlayNetwork != StateOwner || OverlayNetwork->EnvelopeHead != Work) return;
		Admission.Reset();
		OverlayNetwork->bEnvelopeDecode = true;
		OverlayNetwork->EnvelopeHead.Reset();
	}
	else if (!BindingError.IsEmpty()) Fail(BindingError);
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
	const auto StateOwner = OverlayNetwork;
	const auto& PendingResult = StateOwner->DecodedEnvelopeResult.GetValue();
	if (StateOwner->DecodedEnvelope->IsServer() && StateOwner->DecodedEnvelope->GetMessage().Kind == EVoxelMessage::Hello &&
		PendingResult.bSuccess && !PendingResult.bCanceled && !bRejected &&
		TaskOwnerId == PendingResult.Stamp.OwnerId && TaskOwnerIncarnation == PendingResult.Stamp.OwnerIncarnation &&
		WorldModule->GetWorldEpoch() == PendingResult.Stamp.WorldEpoch)
	{
		HandleHelloPayload(StateOwner->DecodedEnvelope->GetMessage().Payload);
		if (bPendingWelcome) return false;
		if (OverlayNetwork != StateOwner) return true;
	}
	const bool bInvalidation = !StateOwner->DecodedEnvelope->IsServer() &&
		StateOwner->DecodedEnvelope->GetMessage().Kind == EVoxelMessage::RepresentationInvalidate;
	if (bInvalidation && PendingResult.bSuccess && !PendingResult.bCanceled && !bRejected &&
		TaskOwnerId == PendingResult.Stamp.OwnerId && TaskOwnerIncarnation == PendingResult.Stamp.OwnerIncarnation &&
		WorldModule->GetWorldEpoch() == PendingResult.Stamp.WorldEpoch)
	{
		if (!AdvanceDecodedInvalidation()) return false;
		if (OverlayNetwork != StateOwner) return true;
		auto InvalidationLease = MoveTemp(StateOwner->InvalidationLease);
		auto Invalidation = MoveTemp(StateOwner->Invalidation);
		auto Work = MoveTemp(StateOwner->DecodedEnvelope);
		auto Result = MoveTemp(StateOwner->DecodedEnvelopeResult.GetValue());
		StateOwner->DecodedEnvelopeResult.Reset();
		StateOwner->bEnvelopeDecode = false;
		check(StateOwner->EnvelopeBytes >= Work->GetIngressQuotaBytes());
		StateOwner->EnvelopeBytes -= Work->GetIngressQuotaBytes();
		StateOwner->InvalidationCapacityConsumer = {};
		StateOwner->InvalidationCapacityVersion = 0;
		StateOwner->InvalidationCursor = 0;
		StateOwner->bInvalidationDecoded = false;
		StateOwner->bInvalidationWaitingCapacity = false;
		if (!bRejected)
		{
			Result.MarkWorkUsed();
			OnRepresentationInvalidated.Broadcast(Invalidation);
		}
		// 回调可关闭世界；局部原通知先销毁backing，再退原租约。
		Invalidation.Entries.Empty();
		InvalidationLease.Reset();
		return true;
	}

	// Handle 可在帧外关闭并重建世界；移出完成槽后不再引用旧连接状态。
	auto Work = MoveTemp(OverlayNetwork->DecodedEnvelope);
	auto Result = MoveTemp(OverlayNetwork->DecodedEnvelopeResult.GetValue());
	OverlayNetwork->DecodedEnvelopeResult.Reset();
	OverlayNetwork->bEnvelopeDecode = false;
	OverlayNetwork->Invalidation.Entries.Empty();
	OverlayNetwork->InvalidationLease.Reset();
	OverlayNetwork->InvalidationCapacityConsumer = {};
	OverlayNetwork->InvalidationCapacityVersion = 0;
	OverlayNetwork->InvalidationCursor = 0;
	OverlayNetwork->bInvalidationDecoded = false;
	OverlayNetwork->bInvalidationWaitingCapacity = false;
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

bool UVoxelModuleNetworkComponent::AdvanceDecodedInvalidation()
{
	const auto State = OverlayNetwork;
	auto* WorldModule = Module.Get();
	if (!State || !State->DecodedEnvelope || !State->DecodedEnvelopeResult.IsSet() || !WorldModule) return false;
	const auto& Message = State->DecodedEnvelope->GetMessage();
	if (!bReady || (bWelcomed && Message.Session != Session))
	{
		Fail(!bReady ? TEXT("Voxel message arrived before Ready") : TEXT("Voxel session identity mismatch"));
		return true;
	}
	const auto Resources = State->Resources;
	if (!Resources) return false;
	if (State->bInvalidationWaitingCapacity)
	{
		uint64 CurrentVersion = 0;
		if (!Resources->TryGetReleaseVersionExcludingOwner(State->InvalidationCapacityConsumer, CurrentVersion) ||
			CurrentVersion <= State->InvalidationCapacityVersion) return false;
		State->bInvalidationWaitingCapacity = false;
	}
	FVoxelGenerationRequiredSourceScope Sources(Resources.Get(), State.Get(),
		[](const void* InContext, FVoxelGenerationRequiredSourceScope::FVisit InVisit)
		{
			const auto& Source = *static_cast<const FVoxelOverlayNetworkState*>(InContext);
			const auto VisitLease = [&Source, InVisit](const TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& Lease)
			{
				if (Lease && Lease->IsOwnedBy(*Source.Resources)) FVoxelGenerationRequiredSourceScope::VisitLease(Lease.Get(), InVisit);
			};
			VisitLease(Source.ResourceLease);
			VisitLease(Source.InvalidationLease);
			Source.DecodedEnvelope->VisitResourceLeases(VisitLease);
		});
	FString Error;
	if (!State->bInvalidationDecoded)
	{
		FVoxelByteReader Reader(Message.Payload);
		const uint16 Count = Reader.U16();
		if (Count == 0 || Count > 256 || Message.Payload.Num() != 2 + Count * VoxelGeometryWire::HeaderBytes)
		{
			Fail(TEXT("Voxel representation invalidation header is invalid"));
			return true;
		}
		const auto& Stamp = State->DecodedEnvelopeResult.GetValue().Stamp;
		const auto Consumer = FVoxelGenerationConsumerKey::Section(Stamp.WorldEpoch, Stamp.OwnerId, Stamp.OwnerIncarnation, {}, Stamp.Token);
		const uint64 BeforeAdmission = Resources->ReleaseVersionExcludingOwner(Consumer);
		FVoxelGenerationRequiredCapacity Required;
		if (!FVoxelGenerationRequiredSourceScope::Collect(*Resources, nullptr, Required, Error))
		{
			Fail(Error);
			return true;
		}
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
		const uint64 Bytes = VoxelGenerationAllocation::Private::ArrayReserveBytes<FVoxelRepresentationInvalidation, FDefaultAllocator>(Count);
		const auto Admission = Resources->TryAcquire(Consumer, EVoxelTerrainResourcePool::Input, Bytes, Lease, 0,
			Required.Bytes[int32(EVoxelTerrainResourcePool::Input)], Required.LeaseCount);
		if (Admission != EVoxelTerrainResourceAdmission::Granted)
		{
			if (Admission != EVoxelTerrainResourceAdmission::CapacityBlocked)
			{
				Fail(Admission == EVoxelTerrainResourceAdmission::CapacityUnachievable ?
					TEXT("Oversized: original invalidation native input") : TEXT("InvalidResource: original invalidation input domain"));
				return true;
			}
			State->InvalidationCapacityConsumer = Consumer;
			State->InvalidationCapacityVersion = BeforeAdmission;
			State->bInvalidationWaitingCapacity = true;
			return false;
		}
		FVoxelRepresentationInvalidate Candidate;
		if (!FVoxelNetworkCodec::DecodeRepresentationInvalidate(Message.Payload, Candidate, Error))
		{
			Fail(Error);
			return true;
		}
		if (VoxelGenerationAllocation::AllocatedBytes(Candidate.Entries) != Bytes)
		{
			Fail(TEXT("InvalidResource: invalidation codec native allocation contract"));
			return true;
		}
		State->Invalidation = MoveTemp(Candidate);
		State->InvalidationLease = MoveTemp(Lease);
		State->bInvalidationDecoded = true;
	}
	const auto Consumer = WorldModule->GetRuntime()->GetChangeHierarchy().GetControlConsumer();
	const uint64 BeforeAdmission = Resources->ReleaseVersionExcludingOwner(Consumer);
	if (!WorldModule->InvalidateRemoteRepresentations(State->Invalidation, State->InvalidationCursor, Error))
	{
		if (Error.StartsWith(TEXT("PlanningPending"))) return false;
		if (!Error.StartsWith(TEXT("CapacityBlocked")))
		{
			Fail(Error);
			return true;
		}
		State->InvalidationCapacityConsumer = Consumer;
		State->InvalidationCapacityVersion = BeforeAdmission;
		State->bInvalidationWaitingCapacity = true;
		return false;
	}
	return true;
}

bool UVoxelModuleNetworkComponent::HandleWelcome(const FVoxelWireMessage& InMessage)
{
	return HandleWelcomePayload(InMessage.Payload, InMessage.Session);
}

bool UVoxelModuleNetworkComponent::HandleWelcomePayload(const TConstArrayView<uint8> InPayload, const FGuid& InSession)
{
	UVoxelModule* VoxelModule = Module.Get();
	if (!VoxelModule) return false;
	FVoxelNetworkWelcome Welcome;
	FString Error;
	if (!FVoxelNetworkCodec::DecodeWelcome(InPayload, Welcome) ||
		Welcome.EchoNonce != ClientNonce || Welcome.Session != InSession)
	{
		Fail(TEXT("Voxel welcome is invalid"));
		return false;
	}
	FVoxelByteReader IdentityReader(InPayload);
	IdentityReader.U64();
	IdentityReader.Guid();
	IdentityReader.U64();
	const auto ManifestBytes = IdentityReader.BlobView(FVoxelManifestCodec::MaxBytes);
	FVoxelOverlayHash IncomingManifest;
	if (!IdentityReader.End() || !FVoxelOverlayHash::Compute(ManifestBytes, IncomingManifest))
	{
		Fail(TEXT("Voxel welcome manifest identity invalid"));
		return false;
	}
	const auto* PreviousReplica = VoxelModule->GetOverlayReplica();
	const bool bWorldChanged = VoxelModule->GetRuntime() &&
		(!VoxelModule->IsReady() || !PreviousReplica || PreviousReplica->GetManifestIdentity() != IncomingManifest ||
			Session != Welcome.Session || PreviousReplica->GetAuthorityEpoch() != Welcome.AuthorityWorldEpoch);
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
	// StartWorld可结束旧协议；恢复已经验证的同一Welcome控制身份。
	Session = Welcome.Session;
	ClientNonce = Welcome.EchoNonce;
	if (auto* Replica = VoxelModule->GetOverlayReplica())
	{
		Replica->SetAuthority(Welcome.AuthorityWorldEpoch, IncomingManifest, Welcome.Session);
	}
	if (!bPendingReady)
	{
		check(NextTaskSerial != MAX_uint64);
		PendingReadyConsumer = FVoxelGenerationConsumerKey::Section(VoxelModule->GetWorldEpoch(), TaskOwnerId,
			TaskOwnerIncarnation, {}, ++NextTaskSerial);
		PendingReadySession = Session;
		bPendingReadyServer = false;
		bPendingReady = true;
	}
	FinishPendingReady();
	return !bRejected;
}

void UVoxelModuleNetworkComponent::HandleReadyPayload(const TConstArrayView<uint8> InPayload)
{
	auto* VoxelModule = Module.Get();
	if (!VoxelModule) return;
	FVoxelNetworkReady Ready;
	const FVoxelWorldManifest& Manifest = VoxelModule->GetManifest();
	if (!FVoxelNetworkCodec::DecodeReady(InPayload, Ready) ||
		Ready.RecipeHash != Manifest.RecipeHash ||
		Ready.RegistryHash != Manifest.RegistryHash ||
		Ready.BaseSampleHash != Manifest.BaseSampleHash)
	{
		Fail(TEXT("Voxel ready identity mismatch"));
		return;
	}
	if (!bReady && !bPendingReady)
	{
		check(NextTaskSerial != MAX_uint64);
		PendingReadyConsumer = FVoxelGenerationConsumerKey::Section(VoxelModule->GetWorldEpoch(), TaskOwnerId,
			TaskOwnerIncarnation, {}, ++NextTaskSerial);
		PendingReadySession = Session;
		bPendingReadyServer = true;
		bPendingReady = true;
	}
	if (!VoxelModule->GetTerrainFrameContext().IsActive()) FinishPendingReady();
	else if (bPendingReady) VoxelModule->QueueTerrainWorldControl(this);
	return;
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
		HandleHelloPayload(InMessage.Payload);
		return;
	}
	if (!bInServer && InMessage.Kind == EVoxelMessage::Welcome)
	{
		HandleWelcome(InMessage);
		return;
	}
	if (bInServer && InMessage.Kind == EVoxelMessage::Ready)
	{
		HandleReadyPayload(InMessage.Payload);
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
		Fail(TEXT("InvalidResource: representation invalidation requires its original decoded envelope slot"));
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
		Task.GenerationConsumer = Consumer;
		Task.SetRequiredBindingInput(PageBuild->MetadataLease->Bytes(), 1);
		auto GetBlockReason = [PageBuild] { return PageBuild->BlockReason(); };
		auto Execute = [PageBuild](const TAtomic<bool>& InCancel) { return PageBuild->Advance(InCancel); };
		const TWeakObjectPtr<UVoxelModuleNetworkComponent> WeakThis(this);
		const FGuid RequestSession = Session;
		auto Apply = [WeakThis, RequestSession, Reply, PageBuild](FVoxelTaskResult&& InResult) mutable
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
		if (!Task.BindGetBlockReason(Resources.ToSharedRef(), Consumer, MoveTemp(GetBlockReason), Error) ||
			!Task.BindExecute(Resources.ToSharedRef(), Consumer, MoveTemp(Execute), Error) ||
			!Task.BindApply(Resources.ToSharedRef(), Consumer, MoveTemp(Apply), Error) ||
			!VoxelModule->EnqueueNetworkRepresentationTask(MoveTemp(Task)))
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


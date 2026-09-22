#include "Voxel/Network/VoxelModuleNetworkComponent.h"

#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Network/VoxelNetworkCodec.h"
#include "Voxel/Network/VoxelRepresentationSync.h"
#include "Voxel/Runtime/VoxelWorldRuntime.h"
#include "Voxel/Serialization/VoxelBinaryCodec.h"
#include "Voxel/Streaming/VoxelEmergeManager.h"
#include "Voxel/Streaming/VoxelInterestManager.h"
#include "Voxel/VoxelModule.h"

namespace
{
	constexpr int32 ChangeRegionSide = 8;

	FIntVector ToChangeRegion(const FIntVector& InSection)
	{
		return FIntVector(
			VoxelGeneration::FloorDivide(InSection.X, ChangeRegionSide),
			VoxelGeneration::FloorDivide(InSection.Y, ChangeRegionSide),
			VoxelGeneration::FloorDivide(InSection.Z, ChangeRegionSide));
	}

	uint64 HashChangeMask(const TArray<uint64>& InMask)
	{
		uint64 Hash = 1469598103934665603ull;
		for (const uint64 Word : InMask)
		{
			Hash ^= Word;
			Hash *= 1099511628211ull;
		}
		return FMath::Max<uint64>(1, Hash);
	}

	auto ResolveWireRepresentationBounds(
		EVoxelRepresentationWireType InType,
		const FVoxelRepresentationWireKey& InKey,
		const FVoxelGenerationSettings& InSettings) -> FVoxelGenerationBounds
	{
		if (InType == EVoxelRepresentationWireType::VoxelProxy)
		{
			return FVoxelViewKey { InKey.Coordinate, InKey.Level }.GetBounds();
		}
		const int32 Side = InType == EVoxelRepresentationWireType::SurfaceProxy ?
			32 * (1 << InKey.Level) : 32 * (64 << InKey.Level);
		const FIntVector Min(
			InKey.Coordinate.X * Side,
			InKey.Coordinate.Y * Side,
			InSettings.MinZ);
		return { Min, FIntVector(Min.X + Side, Min.Y + Side, InSettings.MaxZ) };
	}

	uint64 RepresentationRevision(
		const FVoxelWorldRuntime& InRuntime,
		const EVoxelRepresentationWireType InType,
		const FVoxelRepresentationWireKey& InKey)
	{
		switch (InType)
		{
		case EVoxelRepresentationWireType::VoxelProxy:
			return InRuntime.GetChangeHierarchy().GetVoxelProxyRevision({ InKey.Coordinate, InKey.Level });
		case EVoxelRepresentationWireType::SurfaceProxy:
			return InRuntime.GetChangeHierarchy().GetSurfaceRevision({ FIntPoint(InKey.Coordinate.X, InKey.Coordinate.Y), InKey.Level });
		case EVoxelRepresentationWireType::MacroTerrain:
			return InRuntime.GetChangeHierarchy().GetMacroRevision({ FIntPoint(InKey.Coordinate.X, InKey.Coordinate.Y), InKey.Level });
		default:
			return 0;
		}
	}

	bool IsRepresentationInRange(
		const FVector& InObserverCell,
		const EVoxelRepresentationWireType InType,
		const FVoxelGenerationBounds& InBounds)
	{
		const FVector Center = FVector(InBounds.Min + InBounds.Max) * 0.5;
		const double Radius = InType == EVoxelRepresentationWireType::VoxelProxy ? 512.0 :
			(InType == EVoxelRepresentationWireType::SurfaceProxy ? 4096.0 : 32768.0);
		return FVector::DistSquared(InObserverCell, Center) <= FMath::Square(Radius + 0.5 * (InBounds.Max.X - InBounds.Min.X));
	}
}

UVoxelModuleNetworkComponent::UVoxelModuleNetworkComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

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
	if (!VoxelModule || bRejected)
	{
		return;
	}
	const double Now = FPlatformTime::Seconds();
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
	ApplyPendingSnapshots();
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
	if (!IsSessionReady() || !GetOwner() || GetOwner()->HasAuthority() ||
		PendingRepresentationRequests.Num() >= NetworkSettings.MaxRepresentationRequests)
	{
		return false;
	}
	for (const TPair<uint64, FVoxelRepresentationRequest>& Pair : PendingRepresentationRequests)
	{
		if (Pair.Value.Type == InType && Pair.Value.Key == InKey)
		{
			return true;
		}
	}
	FVoxelRepresentationRequest Request;
	Request.Type = InType;
	Request.Key = InKey;
	Request.KnownRevision = InKnownRevision;
	Request.Serial = AllocateRequestId();
	TArray<uint8> Payload;
	if (Request.Serial == 0 || !FVoxelNetworkCodec::EncodeRepresentationRequest(Request, Payload) ||
		!Send(EVoxelMessage::RepresentationRequest, Payload, EVoxelTransferPriority::Normal))
	{
		return false;
	}
	PendingRepresentationRequests.Add(Request.Serial, Request);
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
	UVoxelModule* VoxelModule = UVoxelModule::Find(GetWorld());
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
	}
	Module = VoxelModule;
	CommitHandle = VoxelModule->OnBlocksCommitted.AddUObject(this, &UVoxelModuleNetworkComponent::OnCommit);
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		Session = VoxelModule->GetSessionId();
	}
}

void UVoxelModuleNetworkComponent::ResetProtocol()
{
	Transfer.Reset();
	PendingSnapshots.Reset();
	GameplayInterest.Reset();
	SentChangeSummaryRevisions.Reset();
	RepresentationSubscriptions.Reset();
	PendingRepresentationRequests.Reset();
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
	const FGuid EnvelopeSession = InKind == EVoxelMessage::Hello ? FGuid() : Session;
	return FVoxelNetworkCodec::Encode(InKind, EnvelopeSession, InPayload, Message) &&
		Transfer.Enqueue(InPriority, MoveTemp(Message));
}

void UVoxelModuleNetworkComponent::Receive(const FVoxelRPCPacket& InPacket, const bool bInServer)
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
	if (!Transfer.Receive(InPacket.Bytes, Now, Completed))
	{
		Fail(TEXT("Malformed voxel transfer fragment"));
		return;
	}
	if (Completed.IsEmpty())
	{
		return;
	}
	FVoxelWireMessage Message;
	if (!FVoxelNetworkCodec::Decode(Completed, Message))
	{
		Fail(TEXT("Malformed voxel V4 envelope"));
		return;
	}
	if (Message.Kind != EVoxelMessage::Hello && bWelcomed && Message.Session != Session)
	{
		Fail(TEXT("Voxel session identity mismatch"));
		return;
	}
	Handle(Message, bInServer);
}

void UVoxelModuleNetworkComponent::Handle(
	const FVoxelWireMessage& InMessage,
	const bool bInServer)
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
		FVoxelNetworkWelcome Welcome;
		FString Error;
		if (!FVoxelNetworkCodec::DecodeWelcome(InMessage.Payload, Welcome) ||
			Welcome.EchoNonce != ClientNonce)
		{
			Fail(TEXT("Voxel welcome is invalid"));
			return;
		}
		Session = Welcome.Session;
		if (!VoxelModule->IsReady() && !VoxelModule->StartWorld(Welcome.Manifest, true, Error))
		{
			Fail(Error.IsEmpty() ? TEXT("Voxel remote world could not start") : Error);
			return;
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
			return;
		}
		bWelcomed = true;
		bReady = true;
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
	if (!bInServer && InMessage.Kind == EVoxelMessage::ChangeSummary)
	{
		FVoxelRegionChangeSummary Summary;
		FString Error;
		if (!FVoxelNetworkCodec::DecodeChangeSummary(InMessage.Payload, Summary, Error))
		{
			Fail(Error.IsEmpty() ? TEXT("Invalid voxel change summary") : Error);
			return;
		}
		VoxelModule->GetRuntime()->GetChangeIndex().SetRegionMask(Summary.Region, Summary.ModifiedMask);
		for (int32 Bit = 0; Bit < ChangeRegionSide * ChangeRegionSide * ChangeRegionSide; ++Bit)
		{
			const bool bModified = (Summary.ModifiedMask[Bit >> 6] & (1ull << (Bit & 63))) != 0;
			const FIntVector Local(
				Bit % ChangeRegionSide,
				(Bit / ChangeRegionSide) % ChangeRegionSide,
				Bit / (ChangeRegionSide * ChangeRegionSide));
			VoxelModule->SetRemoteChangeState(
				Summary.Region * ChangeRegionSide + Local,
				bModified ? EVoxelSectionChangeState::Modified : EVoxelSectionChangeState::Natural);
		}
		return;
	}

	if (!bInServer && InMessage.Kind == EVoxelMessage::SectionState)
	{
		FVoxelSectionState State;
		if (!FVoxelNetworkCodec::DecodeSectionState(InMessage.Payload, State))
		{
			Fail(TEXT("Invalid voxel section state"));
			return;
		}
		VoxelModule->SetRemoteChangeState(
			State.Section,
			State.State == EVoxelSectionWireState::Natural ?
				EVoxelSectionChangeState::Natural : EVoxelSectionChangeState::Modified);
		return;
	}
	if (!bInServer && InMessage.Kind == EVoxelMessage::SectionSnapshot)
	{
		FVoxelNetworkSectionSnapshot Snapshot;
		FString Error;
		if (!FVoxelNetworkCodec::DecodeSectionSnapshot(InMessage.Payload, Snapshot, Error))
		{
			Fail(Error.IsEmpty() ? TEXT("Invalid voxel section snapshot") : Error);
			return;
		}
		PendingSnapshots.Add(Snapshot.Section, MoveTemp(Snapshot));
		return;
	}
	if (!bInServer && InMessage.Kind == EVoxelMessage::SectionPatch)
	{
		FVoxelNetworkPatchBatch WireBatch;
		FString Error;
		if (!FVoxelNetworkCodec::DecodePatchBatch(InMessage.Payload, WireBatch, Error))
		{
			Fail(Error.IsEmpty() ? TEXT("Invalid voxel patch batch") : Error);
			return;
		}
		FVoxelEditBatch Batch;
		Batch.TransactionId = WireBatch.TransactionId;
		Batch.Sections = MoveTemp(WireBatch.Sections);
		if (!VoxelModule->GetRuntime()->ApplyRemotePatchBatch(Batch, Error))
		{
			TArray<uint8> Payload;
			FVoxelByteWriter Writer(256);
			Writer.U16(Batch.Sections.Num());
			for (const FVoxelSectionPatch& Patch : Batch.Sections)
			{
				FVoxelNetworkCodec::WriteVector(Writer, Patch.Section);
			}
			if (Writer.Finish(Payload))
			{
				Send(EVoxelMessage::Resync, Payload, EVoxelTransferPriority::Critical);
			}
			return;
		}
		VoxelModule->PublishProjectEdit(Batch);
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
	if (bInServer && InMessage.Kind == EVoxelMessage::Resync)
	{
		FVoxelByteReader Reader(InMessage.Payload);
		const uint16 Count = Reader.U16();
		if (Count > 32)
		{
			Fail(TEXT("Voxel resync section count exceeded"));
			return;
		}
		for (uint16 Index = 0; Index < Count; ++Index)
		{
			SendSectionSnapshot(FVoxelNetworkCodec::ReadVector(Reader));
		}
		if (!Reader.End())
		{
			Fail(TEXT("Malformed voxel resync request"));
		}
		return;
	}
	if (!bInServer && InMessage.Kind == EVoxelMessage::RepresentationReply)
	{
		FVoxelRepresentationReply Reply;
		FString Error;
		if (!FVoxelNetworkCodec::DecodeRepresentationReply(InMessage.Payload, Reply, Error))
		{
			Fail(Error.IsEmpty() ? TEXT("Invalid voxel representation reply") : Error);
			return;
		}
		const FVoxelRepresentationRequest* Request = PendingRepresentationRequests.Find(Reply.Serial);
		if (!Request || Request->Type != Reply.Type || !(Request->Key == Reply.Key))
		{
			Fail(TEXT("Voxel representation reply does not match an active request"));
			return;
		}
		PendingRepresentationRequests.Remove(Reply.Serial);
		if (!VoxelModule->ApplyRemoteRepresentation(Reply, Error))
		{
			Fail(Error.IsEmpty() ? TEXT("Voxel representation reply could not be applied") : Error);
			return;
		}
		OnRepresentationReply.Broadcast(Reply);
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
		AActor* Observer = GetAuthorizedObserver();
		FVoxelWorldRuntime* Runtime = VoxelModule->GetRuntime();
		if (!Observer || !Runtime || Request.Key.Level > 10)
		{
			Fail(TEXT("Voxel representation request is outside supported limits"));
			return;
		}
		const FVoxelGenerationBounds Bounds = ResolveWireRepresentationBounds(
			Request.Type,
			Request.Key,
			VoxelModule->GetManifest().Settings);
		const FVector ObserverCell = Observer->GetActorLocation() / VoxelModule->BlockSize();
		if (!IsRepresentationInRange(ObserverCell, Request.Type, Bounds))
		{
			Fail(TEXT("Voxel representation request is outside the authorized visual radius"));
			return;
		}
		const uint8 TypeBit = 1u << (static_cast<uint8>(Request.Type) - 1);
		if (!RepresentationSubscriptions.Contains(Request.Key) &&
			RepresentationSubscriptions.Num() >= NetworkSettings.MaxRepresentationRequests)
		{
			Fail(TEXT("Voxel representation subscription limit exceeded"));
			return;
		}
		RepresentationSubscriptions.FindOrAdd(Request.Key) |= TypeBit;

		TArray<FIntVector> ModifiedSections;
		Runtime->GetChangeIndex().Enumerate(Bounds, ModifiedSections);
		const uint64 CurrentRevision = RepresentationRevision(*Runtime, Request.Type, Request.Key);
		FVoxelRepresentationReply Reply;
		Reply.Type = Request.Type;
		Reply.Key = Request.Key;
		Reply.Serial = Request.Serial;
		Reply.Revision = CurrentRevision;
		FString Error;
		if (ModifiedSections.IsEmpty() || Request.KnownRevision == CurrentRevision)
		{
			Reply.Kind = EVoxelRepresentationReplyKind::Unchanged;
			TArray<uint8> Payload;
			if (FVoxelNetworkCodec::EncodeRepresentationReply(Reply, Payload, Error))
			{
				Send(EVoxelMessage::RepresentationReply, Payload, EVoxelTransferPriority::Bulk);
			}
			return;
		}

		FVoxelRepresentationBuildInput BuildInput;
		if (!FVoxelRepresentationSync::PrepareServerBuild(
			*VoxelModule,
			Request,
			BuildInput,
			Error))
		{
			Reply.Kind = EVoxelRepresentationReplyKind::Retry;
			TArray<uint8> Payload;
			if (FVoxelNetworkCodec::EncodeRepresentationReply(Reply, Payload, Error))
			{
				Send(EVoxelMessage::RepresentationReply, Payload, EVoxelTransferPriority::Bulk);
			}
			return;
		}

		FVoxelTaskRequest Task;
		Task.Kind = EVoxelTaskKind::NetworkRepresentation;
		Task.WorkClass = EVoxelWorkClass::Exploration;
		Task.Stamp.WorldEpoch = VoxelModule->GetWorldEpoch();
		Task.Stamp.Token = Request.Serial ^ static_cast<uint64>(reinterpret_cast<UPTRINT>(this));
		Task.Stamp.Revision = CurrentRevision;
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
		Task.ReservedBytes = 32ull * 1024ull * 1024ull;
		Task.InputBytes = BuildInput.Overlays.GetAllocatedBytes();
		Task.Execute = [BuildInput = MoveTemp(BuildInput)](const TAtomic<bool>& InCancel)
		{
			FVoxelTaskResult Result;
			Result.bSuccess = FVoxelRepresentationSync::BuildServerData(
				BuildInput,
				Result.Payload,
				Result.Error,
				&InCancel);
			return Result;
		};
		const TWeakObjectPtr<UVoxelModuleNetworkComponent> WeakThis(this);
		const FGuid RequestSession = Session;
		Task.Apply = [WeakThis, RequestSession, Reply](FVoxelTaskResult&& InResult) mutable
		{
			UVoxelModuleNetworkComponent* Component = WeakThis.Get();
			if (!Component || Component->Session != RequestSession || !Component->bReady)
			{
				return;
			}
			Reply.Kind = InResult.bSuccess && !InResult.bCanceled ?
				EVoxelRepresentationReplyKind::Data : EVoxelRepresentationReplyKind::Retry;
			Reply.Data = InResult.bSuccess ? MoveTemp(InResult.Payload) : TArray<uint8>();
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
	UVoxelModule* VoxelModule = Module.Get();
	AActor* Observer = GetAuthorizedObserver();
	if (!VoxelModule || !Observer || !VoxelModule->IsReady())
	{
		return;
	}
	FVoxelStreamingSource Source;
	const FVector Position = Observer->GetActorLocation() / VoxelModule->BlockSize();
	Source.Center = FIntVector(
		FMath::FloorToInt(Position.X),
		FMath::FloorToInt(Position.Y),
		FMath::FloorToInt(Position.Z));
	Source.Direction = Observer->GetActorForwardVector();
	Source.RenderMode = EVoxelStreamingRenderMode::None;
	Source.ExactRadius = 16;
	Source.CollisionRadius = 8;
	Source.SimulationRadius = 6;
	if (SourceId.IsValid())
	{
		VoxelModule->UpdateSource(SourceId, Source);
	}
	else
	{
		SourceId = VoxelModule->RegisterSource(this, Source);
	}

	const FVoxelInterestManager InterestManager;
	const FVoxelInterestSet ClientInterest = InterestManager.Compute(
		MakeArrayView(&Source, 1),
		VoxelModule->GetManifest(),
		FVoxelViewSettings());
	TSet<FIntVector> Next;
	TSet<FIntVector> SummaryRegions;
	for (const TPair<FIntVector, FVoxelExactDemand>& Pair : ClientInterest.Exact)
	{
		if (Pair.Value.bData)
		{
			Next.Add(Pair.Key);
			SummaryRegions.Add(ToChangeRegion(Pair.Key));
			if (!GameplayInterest.Contains(Pair.Key))
			{
				SendSectionState(Pair.Key);
			}
		}
	}
	for (const FIntVector& Region : SummaryRegions)
	{
		FVoxelRegionChangeSummary Summary;
		Summary.Region = Region;
		VoxelModule->GetRuntime()->GetChangeIndex().GetRegionMask(Region, Summary.ModifiedMask);
		Summary.Revision = HashChangeMask(Summary.ModifiedMask);
		const uint64* SentRevision = SentChangeSummaryRevisions.Find(Region);
		if (SentRevision && *SentRevision == Summary.Revision)
		{
			continue;
		}
		TArray<uint8> Payload;
		FString Error;
		if (FVoxelNetworkCodec::EncodeChangeSummary(Summary, Payload, Error) &&
			Send(EVoxelMessage::ChangeSummary, Payload, EVoxelTransferPriority::Normal))
		{
			SentChangeSummaryRevisions.Add(Region, Summary.Revision);
		}
	}
	for (auto Iterator = SentChangeSummaryRevisions.CreateIterator(); Iterator; ++Iterator)
	{
		if (!SummaryRegions.Contains(Iterator.Key()))
		{
			Iterator.RemoveCurrent();
		}
	}
	GameplayInterest = MoveTemp(Next);
	LastInterest = InNow;
}

void UVoxelModuleNetworkComponent::SendSectionState(const FIntVector& InSection)
{
	UVoxelModule* VoxelModule = Module.Get();
	if (!VoxelModule)
	{
		return;
	}
	FVoxelSectionState State;
	State.Section = InSection;
	const FVoxelSection* Section = VoxelModule->GetRuntime()->FindSection(InSection);
	if (Section && Section->Status == EVoxelSectionStatus::DataReady)
	{
		State.Revision = Section->CommittedRevision;
		State.State = Section->Overlay.IsEmpty() && Section->Entities.IsEmpty() ?
			EVoxelSectionWireState::Natural : EVoxelSectionWireState::Modified;
	}
	else
	{
		State.State = VoxelModule->GetRuntime()->GetChangeIndex().IsModified(InSection) ?
			EVoxelSectionWireState::Modified : EVoxelSectionWireState::Natural;
	}
	TArray<uint8> Payload;
	if (FVoxelNetworkCodec::EncodeSectionState(State, Payload))
	{
		Send(EVoxelMessage::SectionState, Payload, EVoxelTransferPriority::Normal);
	}
	if (State.State == EVoxelSectionWireState::Modified && Section)
	{
		SendSectionSnapshot(InSection);
	}
}

void UVoxelModuleNetworkComponent::SendSectionSnapshot(const FIntVector& InSection)
{
	UVoxelModule* VoxelModule = Module.Get();
	const FVoxelSection* Section = VoxelModule && VoxelModule->GetRuntime() ?
		VoxelModule->GetRuntime()->FindSection(InSection) : nullptr;
	if (!Section || Section->Status != EVoxelSectionStatus::DataReady)
	{
		return;
	}
	FVoxelNetworkSectionSnapshot Snapshot;
	Snapshot.Section = InSection;
	Snapshot.Revision = Section->CommittedRevision;
	for (const TPair<int32, FVoxelBlockState>& Pair : Section->Overlay)
	{
		FVoxelSectionCellEdit Edit;
		Edit.CellIndex = Pair.Key;
		Edit.State = Pair.Value;
		Snapshot.Blocks.Add(Edit);
	}
	for (const TPair<int32, FVoxelBlockEntityState>& Pair : Section->Entities)
	{
		FVoxelEntityWrite Write;
		Write.CellIndex = Pair.Key;
		Write.Value = Pair.Value;
		Snapshot.Entities.Add(MoveTemp(Write));
	}
	TArray<uint8> Payload;
	FString Error;
	if (FVoxelNetworkCodec::EncodeSectionSnapshot(Snapshot, Payload, Error))
	{
		Send(EVoxelMessage::SectionSnapshot, Payload, EVoxelTransferPriority::Normal);
	}
}

void UVoxelModuleNetworkComponent::ApplyPendingSnapshots()
{
	UVoxelModule* VoxelModule = Module.Get();
	if (!VoxelModule || VoxelModule->IsAuthority() || !VoxelModule->GetRuntime())
	{
		return;
	}
	for (auto Iterator = PendingSnapshots.CreateIterator(); Iterator; ++Iterator)
	{
		FVoxelSection* Section = VoxelModule->GetRuntime()->FindSection(Iterator.Key());
		if (!Section || Section->Status != EVoxelSectionStatus::BaseReady)
		{
			continue;
		}
		TMap<int32, FVoxelBlockState> Overlay;
		TMap<int32, FVoxelBlockEntityState> Entities;
		for (const FVoxelSectionCellEdit& Edit : Iterator.Value().Blocks)
		{
			Overlay.Add(Edit.CellIndex, Edit.State);
		}
		for (const FVoxelEntityWrite& Write : Iterator.Value().Entities)
		{
			if (!Write.bRemove)
			{
				Entities.Add(Write.CellIndex, Write.Value);
			}
		}
		FString Error;
		if (!VoxelModule->GetRuntime()->PublishFinal(
			Iterator.Key(),
			Iterator.Value().Revision,
			Overlay,
			Entities,
			Error))
		{
			Fail(Error.IsEmpty() ? TEXT("Voxel snapshot apply failed") : Error);
			return;
		}
		FVoxelEditBatch Batch;
		FVoxelSectionPatch Patch;
		Patch.Section = Iterator.Key();
		Patch.ToRevision = Iterator.Value().Revision;
		Batch.Sections.Add(Patch);
		VoxelModule->PublishProjectEdit(Batch);
		Iterator.RemoveCurrent();
	}
}

void UVoxelModuleNetworkComponent::OnCommit(const FVoxelEditBatch& InBatch)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !bReady)
	{
		return;
	}
	UVoxelModule* VoxelModule = Module.Get();
	if (!VoxelModule || !VoxelModule->GetRuntime())
	{
		return;
	}
	FVoxelRepresentationInvalidate Invalidate;
	for (const TPair<FVoxelRepresentationWireKey, uint8>& Subscription : RepresentationSubscriptions)
	{
		bool bAffected = false;
		uint64 Revision = 0;
		for (const FVoxelSectionPatch& Patch : InBatch.Sections)
		{
			const FVoxelChangeHierarchy& Hierarchy = VoxelModule->GetRuntime()->GetChangeHierarchy();
			if ((Subscription.Value & 1u) != 0)
			{
				const FVoxelViewKey Key { Subscription.Key.Coordinate, Subscription.Key.Level };
				bAffected |= Hierarchy.AffectsVoxelProxy(Key, Patch.Section);
				Revision = FMath::Max(Revision, Hierarchy.GetVoxelProxyRevision(Key));
			}
			if ((Subscription.Value & 2u) != 0)
			{
				const FVoxelSurfaceTileKey Key {
					FIntPoint(Subscription.Key.Coordinate.X, Subscription.Key.Coordinate.Y),
					Subscription.Key.Level };
				bAffected |= Hierarchy.AffectsSurface(Key, Patch.Section);
				Revision = FMath::Max(Revision, Hierarchy.GetSurfaceRevision(Key));
			}
			if ((Subscription.Value & 4u) != 0)
			{
				const FVoxelMacroTileKey Key {
					FIntPoint(Subscription.Key.Coordinate.X, Subscription.Key.Coordinate.Y),
					Subscription.Key.Level };
				bAffected |= Hierarchy.AffectsMacro(Key, Patch.Section);
				Revision = FMath::Max(Revision, Hierarchy.GetMacroRevision(Key));
			}
		}
		if (bAffected)
		{
			Invalidate.Keys.Add(Subscription.Key);
			Invalidate.Revision = FMath::Max(Invalidate.Revision, Revision);
		}
	}
	if (!Invalidate.Keys.IsEmpty())
	{
		Invalidate.Revision = FMath::Max<uint64>(1, Invalidate.Revision);
		TArray<uint8> InvalidatePayload;
		FString InvalidateError;
		if (FVoxelNetworkCodec::EncodeRepresentationInvalidate(
			Invalidate,
			InvalidatePayload,
			InvalidateError))
		{
			Send(EVoxelMessage::RepresentationInvalidate, InvalidatePayload, EVoxelTransferPriority::Normal);
		}
	}
	FVoxelNetworkPatchBatch WireBatch;
	WireBatch.TransactionId = InBatch.TransactionId;
	for (const FVoxelSectionPatch& Patch : InBatch.Sections)
	{
		if (GameplayInterest.Contains(Patch.Section))
		{
			WireBatch.Sections.Add(Patch);
		}
	}
	if (WireBatch.Sections.IsEmpty())
	{
		return;
	}
	TArray<uint8> Payload;
	FString Error;
	if (FVoxelNetworkCodec::EncodePatchBatch(WireBatch, Payload, Error))
	{
		Send(EVoxelMessage::SectionPatch, Payload, EVoxelTransferPriority::Critical);
	}
}

APlayerController* UVoxelModuleNetworkComponent::Controller() const
{
	return Cast<APlayerController>(GetOwner());
}

#include "Voxel/Network/VoxelModuleNetworkComponent.h"

#include "VoxelModuleNetworkOverlayState.h"
#include "Voxel/Network/VoxelNetworkCodec.h"
#include "Voxel/Runtime/VoxelWorldRuntime.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Misc/Crc.h"
#include "Voxel/VoxelModule.h"
#include "GameFramework/PlayerController.h"
#include "Async/Async.h"

namespace
{
	constexpr uint64 OverlayConnectionBytes = 32ull * 1024 * 1024;
	bool LessRegion(const FIntVector& A, const FIntVector& B)
	{
		if (A.X != B.X) return A.X < B.X;
		if (A.Y != B.Y) return A.Y < B.Y;
		return A.Z < B.Z;
	}
	FVoxelOverlayRegionMessage ControlMessage(const FVoxelOverlayNetworkState& State)
	{
		FVoxelOverlayRegionMessage Message;
		Message.WorldEpoch = State.Identity.WorldEpoch;
		Message.ManifestIdentity = State.Identity.Manifest;
		Message.DirectoryEpoch = State.Identity.DirectoryEpoch;
		Message.SubscriptionId = State.Identity.Subscription;
		Message.CoverageTicket = State.Identity.Ticket;
		Message.AuthorityCut = State.Identity.Cut;
		return Message;
	}
}

void UVoxelModuleNetworkComponent::AdvanceTerrainNetwork(FVoxelTerrainFrameContext& Frame)
{
	auto* WorldModule = Module.Get();
	if (!WorldModule || !WorldModule->IsReady() || bRejected || Frame.GetWorldEpoch() != WorldModule->GetWorldEpoch()) return;
	if (!OverlayNetwork)
	{
		auto Resources = WorldModule->GetTerrainFactResources();
		if (!Resources) return;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
		const uint64 Bytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelOverlayNetworkState>() +
			VoxelBoundedAllocation::ArrayBytes<FIntVector>(FVoxelOverlayRegionCodec::MaximumRegions) * 2 +
			VoxelBoundedAllocation::ArrayBytes<FVoxelOverlayAuthorityEditCapture>(256) +
			VoxelBoundedAllocation::ArrayBytes<TSharedPtr<const FVoxelOverlayAuthorityDelta, ESPMode::ThreadSafe>>(256) +
			VoxelBoundedAllocation::MapBytes<FIntVector, uint64>(FVoxelOverlayRegionCodec::MaximumRegions);
		if (!Resources->Acquire(FVoxelGenerationConsumerKey::Section(WorldModule->GetWorldEpoch(), TaskOwnerId,
			TaskOwnerIncarnation, {}, ++NextTaskSerial), EVoxelTerrainResourcePool::Resident, Bytes, Lease)) return;
		OverlayNetwork = VoxelBoundedAllocation::MakeWorkerShared<FVoxelOverlayNetworkState>(Lease);
		OverlayNetwork->Resources = MoveTemp(Resources);
		OverlayNetwork->ResourceLease = MoveTemp(Lease);
		OverlayNetwork->Regions.Reserve(FVoxelOverlayRegionCodec::MaximumRegions);
		OverlayNetwork->RetiringRegions.Reserve(FVoxelOverlayRegionCodec::MaximumRegions);
		OverlayNetwork->Edits.Reserve(256);
		OverlayNetwork->Replay.Reserve(256);
		OverlayNetwork->Acknowledged.Reserve(FVoxelOverlayRegionCodec::MaximumRegions);
		OverlayNetwork->Identity.WorldEpoch = WorldModule->IsAuthority() ? WorldModule->GetWorldEpoch() :
			(WorldModule->GetOverlayReplica() ? WorldModule->GetOverlayReplica()->GetAuthorityEpoch() : 0);
		OverlayNetwork->Identity.DirectoryEpoch = Session;
		FVoxelOverlayRegionCodec::ManifestHash(WorldModule->GetManifest(), OverlayNetwork->Identity.Manifest);
		OverlayNetwork->Identity.Subscription = FGuid::NewGuid();
		if (!Transfer.BindResources(OverlayNetwork->Resources.ToSharedRef(),
			FVoxelGenerationConsumerKey::Section(WorldModule->GetWorldEpoch(), TaskOwnerId, TaskOwnerIncarnation, {}, ++NextTaskSerial)))
		{
			OverlayNetwork.Reset();
			return;
		}
	}
	ReserveRepresentationTables(); // 扩容失败保留原容量，控制流和旧请求继续服务。
	const double Now = FPlatformTime::Seconds();
	const double Deadline = Frame.Deadline(0.1);
	if (FPlatformTime::Seconds() < Deadline) ApplyDecodedEnvelope(false);
	if (!OverlayNetwork || bRejected) return;
	if (FPlatformTime::Seconds() < Deadline)
		Transfer.Tick(Now, [this](const TArray<uint8>& Fragment)
		{
			FVoxelRPCPacket Packet;
			Packet.Bytes = Fragment;
			if (GetOwner() && GetOwner()->HasAuthority()) ClientReceive(Packet);
			else ServerReceive(Packet);
		}, Deadline);
	for (int32 Count = 0; Count < 4 && FPlatformTime::Seconds() < Deadline; ++Count)
	{
		FVoxelOverlayQueuedPacket Packet;
		if (!OverlayNetwork->Packets.Dequeue(Packet)) break;
		OverlayNetwork->PacketBytes -= Packet.ResourceLease->Bytes();
		ProcessPacket(Packet.Packet, Packet.bServer);
		if (!OverlayNetwork || bRejected) return;
	}
	if (FPlatformTime::Seconds() < Deadline) AdvanceEnvelopeDecode(Now);
	if (!GetOwner()) return;
	if (!bWelcomed && !GetOwner()->HasAuthority() && Now - LastHello >= 1.0 && FPlatformTime::Seconds() < Deadline)
	{
		if (ClientNonce == 0) ClientNonce = FPlatformTime::Cycles64() | 1;
		FVoxelNetworkHello Hello;
		Hello.Nonce = ClientNonce;
		Hello.RegistryHash = WorldModule->GetRegistry() ? WorldModule->GetRegistry()->Hash : 0;
		TArray<uint8> Payload;
		if (FVoxelNetworkCodec::EncodeHello(Hello, Payload)) Send(EVoxelMessage::Hello, Payload, EVoxelTransferPriority::Critical);
		LastHello = Now;
	}
	if (!bReady) return;
	if (FPlatformTime::Seconds() < Deadline) AdvancePendingRepresentations(Deadline);
	if (GetOwner()->HasAuthority())
	{
		if (FPlatformTime::Seconds() < Deadline) AdvanceRepresentationSubscriptions(Now, Deadline);
		if (Now - LastInterest >= 0.25 && FPlatformTime::Seconds() < Deadline) RefreshInterest(Now);
	}
	else if (FPlatformTime::Seconds() < Deadline) RefreshOverlaySubscription(Now);
	if (FPlatformTime::Seconds() < Deadline) AdvanceOverlayProtocol(Now, Deadline);
}

bool UVoxelModuleNetworkComponent::SendOverlayControl(const EVoxelMessage Kind, TConstArrayView<FIntVector> Regions)
{
	if (!OverlayNetwork || Regions.IsEmpty() || !OverlayNetwork->Identity.Ticket.IsValid()) return false;
	const auto* WorldModule = Module.Get();
	if (!WorldModule) return false;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Scratch;
	const uint64 Bytes = VoxelBoundedAllocation::ArrayBytes<FVoxelOverlayRegionHead>(Regions.Num()) +
		VoxelBoundedAllocation::InitialGrowBytes<uint8>(FVoxelOverlayRegionCodec::MaximumMessageBytes) * 4;
	if (!OverlayNetwork->Resources->Acquire(FVoxelGenerationConsumerKey::Section(WorldModule->GetWorldEpoch(),
		TaskOwnerId, TaskOwnerIncarnation, Regions[0], ++NextTaskSerial), EVoxelTerrainResourcePool::Working, Bytes, Scratch)) return false;
	auto Message = ControlMessage(*OverlayNetwork);
	Message.Region = Regions[0];
	Message.Heads.Reserve(Regions.Num());
	for (const auto& Key : Regions)
	{
		FVoxelOverlayRegionHead Head;
		Head.Region = Key;
		if (const auto* Replica = WorldModule->GetOverlayReplica()) Replica->GetTicketHead(OverlayNetwork->Identity.Ticket, Key, Head);
		Message.Heads.Add(Head);
	}
	TArray<uint8> Payload;
	return FVoxelOverlayRegionCodec::Encode(Kind, Message, Payload) && Send(Kind, Payload, EVoxelTransferPriority::Normal);
}

void UVoxelModuleNetworkComponent::RefreshOverlaySubscription(const double Now)
{
	auto* WorldModule = Module.Get();
	if (!OverlayNetwork || !WorldModule || !WorldModule->GetOverlayReplica()) return;
	auto& State = *OverlayNetwork;
	auto* Replica = WorldModule->GetOverlayReplica();
	State.Identity.WorldEpoch = Replica->GetAuthorityEpoch();
	State.Identity.DirectoryEpoch = Replica->GetDirectoryEpoch();
	State.Identity.Manifest = Replica->GetManifestIdentity();
	const auto Current = WorldModule->GetCurrentInterestSnapshot();
	const auto Desired = WorldModule->GetDesiredInterestSnapshot();
	if (Current->PublicationRevision == State.SeenCurrentRevision && Desired->PublicationRevision == State.SeenDesiredRevision &&
		!State.Regions.IsEmpty()) return;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Scratch;
	if (!State.Resources->Acquire(FVoxelGenerationConsumerKey::Section(WorldModule->GetWorldEpoch(), TaskOwnerId,
		TaskOwnerIncarnation, {}, ++NextTaskSerial), EVoxelTerrainResourcePool::Working,
		VoxelBoundedAllocation::ArrayBytes<FIntVector>(FVoxelOverlayRegionCodec::MaximumRegions) * 2, Scratch)) return;
	TArray<FIntVector> Scope;
	Scope.Reserve(FVoxelOverlayRegionCodec::MaximumRegions);
	const auto AddBounds = [&Scope](FVoxelGenerationBounds Bounds)
	{
		Bounds.Min -= FIntVector(16);
		Bounds.Max += FIntVector(16);
		TArray<FIntVector> Keys;
		Keys.Reserve(FVoxelOverlayRegionCodec::MaximumRegions);
		if (!FVoxelOverlayRegionCodec::GatherRegions(Bounds, Keys)) return false;
		for (const auto& Key : Keys)
		{
			if (Scope.Contains(Key)) continue;
			if (Scope.Num() == FVoxelOverlayRegionCodec::MaximumRegions) return false;
			Scope.Add(Key);
		}
		return true;
	};
	for (const auto& Snapshot : {Current, Desired})
		for (const auto& Pair : Snapshot->Sources)
		{
			const auto& Source = Pair.Value;
			if (Source.GetCurrentWindow().Bounds.IsValid() && !AddBounds(Source.GetCurrentWindow().Bounds)) return;
			if (Source.GetPendingWindow().IsSet() && !AddBounds(Source.GetPendingWindow()->Bounds)) return;
			for (const auto& Section : Source.DataSections)
			{
				const auto Key = FVoxelOverlayRegionCodec::RegionOf(Section);
				if (!Scope.Contains(Key))
				{
					if (Scope.Num() == FVoxelOverlayRegionCodec::MaximumRegions) return;
					Scope.Add(Key);
				}
			}
		}
	Scope.Sort(LessRegion);
	if (Scope.IsEmpty()) return;
	if (Scope == State.Regions)
	{
		State.SeenCurrentRevision = Current->PublicationRevision;
		State.SeenDesiredRevision = Desired->PublicationRevision;
		return;
	}
	if (State.RetiringTicket.IsValid()) Replica->CancelTicket(State.RetiringTicket);
	State.RetiringTicket = State.Identity.Ticket;
	State.RetiringRegions = State.Regions;
	State.Identity.Ticket = FGuid::NewGuid();
	State.Regions = MoveTemp(Scope);
	if (!Replica->ExpectTicket(State.Identity.Subscription, State.Identity.Ticket, State.Regions) ||
		!SendOverlayControl(EVoxelMessage::SubscribeRegion, State.Regions))
	{
		Replica->CancelTicket(State.Identity.Ticket);
		State.Identity.Ticket = State.RetiringTicket;
		State.RetiringTicket.Invalidate();
		State.Regions = MoveTemp(State.RetiringRegions);
		return;
	}
	State.SeenCurrentRevision = Current->PublicationRevision;
	State.SeenDesiredRevision = Desired->PublicationRevision;
	State.LastControl = State.LastRetry = Now;
	State.LastProgress = 0;
	State.RetryCount = 0;
}

void UVoxelModuleNetworkComponent::DecodeOverlayMessage(const FVoxelWireMessage& Message, const bool bServer)
{
	auto* WorldModule = Module.Get();
	if (!OverlayNetwork || !WorldModule || Message.Payload.Num() > FVoxelOverlayRegionCodec::MaximumMessageBytes) return;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	const uint64 Bytes = VoxelBoundedAllocation::SharedBytes<FVoxelOverlayInboundWork>() +
		VoxelBoundedAllocation::ArrayBytes<uint8>(Message.Payload.Num()) +
		VoxelBoundedAllocation::ArrayBytes<FVoxelOverlayRegionHead>(FVoxelOverlayRegionCodec::MaximumRegions) +
		VoxelBoundedAllocation::ArrayBytes<FVoxelOverlayDirectoryEntry>(FVoxelOverlayRegionCodec::MaximumPageEntries) +
		VoxelBoundedAllocation::ArrayBytes<uint8>(FVoxelOverlayRegionCodec::MaximumPartBytes) + 128;
	if (OverlayNetwork->InboundBytes + Bytes > OverlayConnectionBytes ||
		!OverlayNetwork->Resources->Acquire(FVoxelGenerationConsumerKey::Section(WorldModule->GetWorldEpoch(), TaskOwnerId,
			TaskOwnerIncarnation, {}, ++NextTaskSerial), EVoxelTerrainResourcePool::Input, Bytes, Lease))
	{
		Fail(TEXT("Overlay 连接接收配额不足"));
		return;
	}
	auto Work = MakeShared<FVoxelOverlayInboundWork, ESPMode::ThreadSafe>();
	Work->ResourceLease = MoveTemp(Lease);
	Work->Bytes.Reserve(Message.Payload.Num());
	Work->Bytes.Append(Message.Payload);
	Work->Kind = Message.Kind;
	Work->bServer = bServer;
	OverlayNetwork->InboundBytes += Bytes;
	OverlayNetwork->Inbound.Enqueue(MoveTemp(Work));
}

void UVoxelModuleNetworkComponent::HandleOverlayMessage(const EVoxelMessage Kind, FVoxelOverlayRegionMessage&& Message,
	const bool bServer)
{
	auto* WorldModule = Module.Get();
	if (!OverlayNetwork || !WorldModule) return;
	auto& State = *OverlayNetwork;
	if (Message.WorldEpoch != State.Identity.WorldEpoch || Message.ManifestIdentity != State.Identity.Manifest ||
		Message.DirectoryEpoch != State.Identity.DirectoryEpoch) return;
	if (!bServer)
	{
		FString Error;
		if (auto* Replica = WorldModule->GetOverlayReplica())
			if (!Replica->Receive(Kind, MoveTemp(Message), Error)) State.LastRetry = -10.0;
		return;
	}
	if (Kind == EVoxelMessage::SubscribeRegion || Kind == EVoxelMessage::ResyncRequest)
	{
		if (Kind == EVoxelMessage::ResyncRequest && State.Identity.Ticket == Message.CoverageTicket &&
			(State.bBuild || State.SnapshotEncoder) && FPlatformTime::Seconds() - State.LastRetry < 2.0) return;
		if (Message.Heads.IsEmpty() || Message.Heads.Num() > FVoxelOverlayRegionCodec::MaximumRegions || !GetAuthorizedObserver()) return;
		const FVector ObserverCell = GetAuthorizedObserver()->GetActorLocation() / WorldModule->BlockSize();
		TArray<FVoxelStreamingSource> Authorized;
		WorldModule->CollectStreamingSourcesForOwner(Controller(), Authorized);
		const auto Window = WorldModule->GetViewSettings().Fine.WindowSettings();
		const FVector HalfExtent(Window.RadiusCells + Window.ShiftStepCells + 128 + 16,
			Window.RadiusCells + Window.ShiftStepCells + 128 + 16,
			Window.VerticalHalfExtentCells + Window.VerticalShiftStepCells + 128 + 16);
		for (const auto& Head : Message.Heads)
		{
			const FBox Bounds(FVector(Head.Region * 128), FVector((Head.Region + FIntVector(1)) * 128));
			bool bAllowed = Bounds.Intersect(FBox(ObserverCell - HalfExtent, ObserverCell + HalfExtent));
			for (const auto& Source : Authorized)
				bAllowed |= Bounds.Intersect(FBox(FVector(Source.Center) - HalfExtent, FVector(Source.Center) + HalfExtent));
			if (!bAllowed) { Fail(TEXT("Overlay 订阅超出已授权 Source 的当前或预热范围")); return; }
		}
		State.Identity.Subscription = Message.SubscriptionId;
		State.Identity.Ticket = Message.CoverageTicket;
		if (State.bBuild) WorldModule->CancelTerrainNetworkTask(State.BuildStamp);
		if (State.bEncode) WorldModule->CancelTerrainNetworkTask(State.EncodeStamp);
		++State.BuildSerial;
		State.bBuild = State.bEncode = false;
		State.AuthorityRoot.Reset();
		State.SnapshotEncoder.Reset();
		State.DeltaEncoder.Reset();
		State.Outbound.Reset();
		State.Regions.Reset();
		for (const auto& Head : Message.Heads) State.Regions.Add(Head.Region);
		State.Regions.Sort(LessRegion);
		State.bNeedsSnapshot = true;
		return;
	}
	if (Message.SubscriptionId != State.Identity.Subscription || Message.CoverageTicket != State.Identity.Ticket) return;
	if (Kind == EVoxelMessage::RegionAck)
	{
		for (const auto& Head : Message.Heads)
		{
			const auto* Directory = State.AuthorityRoot ? State.AuthorityRoot->Regions.Find(Head.Region) : nullptr;
			if (!Directory || Head.Sequence > (*Directory)->Head.Sequence) continue;
			if (Head.Sequence == (*Directory)->Head.Sequence && Head.Hash != (*Directory)->Head.Hash)
			{
				State.bNeedsSnapshot = true;
				return;
			}
			State.Acknowledged.FindOrAdd(Head.Region) = FMath::Max(State.Acknowledged.FindRef(Head.Region), Head.Sequence);
		}
		return;
	}
	if (Kind == EVoxelMessage::GapRequest)
	{
		const auto* Retained = State.Replay.FindByPredicate([&Message](const auto& Delta)
		{
			for (const auto& Head : Message.Heads)
				if (Delta->Heads.ContainsByPredicate([&Head](const auto& H) { return H.Region == Head.Region && H.PreviousSequence == Head.Sequence; })) return true;
			return false;
		});
		if (Retained && !State.DeltaEncoder)
		{
			State.EncodingDelta = *Retained;
			State.DeltaEncoder = MakeShared<FVoxelOverlayDeltaEncoder, ESPMode::ThreadSafe>((*Retained).ToSharedRef(),
				State.Resources.ToSharedRef(), FVoxelGenerationConsumerKey::Section(WorldModule->GetWorldEpoch(), TaskOwnerId,
					TaskOwnerIncarnation, {}, ++NextTaskSerial));
		}
		else State.bNeedsSnapshot = true;
	}
}

void UVoxelModuleNetworkComponent::BeginOverlaySnapshot()
{
	auto* WorldModule = Module.Get();
	if (!OverlayNetwork || !WorldModule || !WorldModule->IsAuthority() || OverlayNetwork->Regions.IsEmpty()) return;
	auto& State = *OverlayNetwork;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> CaptureLease;
	const auto Consumer = FVoxelGenerationConsumerKey::Section(WorldModule->GetWorldEpoch(), TaskOwnerId,
		TaskOwnerIncarnation, State.Regions[0], ++NextTaskSerial);
	const uint64 Metadata = VoxelBoundedAllocation::SharedBytes<FVoxelOverlayAuthoritySnapshotBuild>() +
		VoxelBoundedAllocation::ArrayBytes<FVoxelOverlayAuthorityRegionCapture>(State.Regions.Num());
	if (!State.Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Input, Metadata, CaptureLease)) return;
	TArray<FVoxelOverlayAuthorityRegionCapture> Captures;
	Captures.Reserve(State.Regions.Num());
	State.Identity.Cut = WorldModule->GetRuntime()->GetAuthorityCommitCut();
	for (const auto& Key : State.Regions)
	{
		FVoxelOverlayAuthorityRegionCapture Capture;
		Capture.Head.Region = Key;
		Capture.Head.Sequence = WorldModule->GetRuntime()->GetOverlayRegionSequence(Key);
		FString Error;
		if (!WorldModule->CaptureFineOverlays({Key * 128, (Key + FIntVector(1)) * 128}, Capture.Capture, Error))
		{
			UE_LOG(LogTemp, Warning, TEXT("Overlay 切点捕获等待：%s"), *Error);
			return;
		}
		Captures.Add(MoveTemp(Capture));
	}
	const auto Identity = State.Identity;
	const uint64 BuildSerial = ++State.BuildSerial;
	auto Build = MakeShared<FVoxelOverlayAuthoritySnapshotBuild, ESPMode::ThreadSafe>(Identity, MoveTemp(Captures),
		State.Resources.ToSharedRef(), Consumer, MoveTemp(CaptureLease));
	FVoxelTaskRequest Task;
	Task.Kind = EVoxelTaskKind::NetworkRepresentation;
	Task.Stamp.OwnerId = TaskOwnerId;
	Task.Stamp.OwnerIncarnation = TaskOwnerIncarnation;
	Task.Stamp.WorldEpoch = WorldModule->GetWorldEpoch();
	Task.Stamp.Token = ++NextTaskSerial;
	Task.Stamp.Section = State.Regions[0];
	State.BuildStamp = Task.Stamp;
	Task.ServiceBand = EVoxelTerrainServiceBand::P2;
	Task.WorkClass = EVoxelWorkClass::Visible;
	Task.SourceStableKey = TaskOwnerId;
	Task.FirstRunnableSeconds = FPlatformTime::Seconds();
	Task.bResumable = true;
	Task.GetBlockReason = [Build, Resources = State.Resources]
	{
		return Build->IsCapacityBlocked() && Resources->Snapshot().ReleaseVersion <= Build->GetCapacityVersion() ?
			EVoxelTaskBlockReason::Memory : EVoxelTaskBlockReason::None;
	};
	Task.Execute = [Build](const TAtomic<bool>& Cancel) { return Build->Advance(Cancel); };
	Task.Apply = [Weak = TWeakObjectPtr<UVoxelModuleNetworkComponent>(this), Identity, BuildSerial](FVoxelTaskResult&& Result)
	{
		auto* Component = Weak.Get();
		if (!Component || !Component->OverlayNetwork || Component->TaskOwnerId != Result.Stamp.OwnerId ||
			Component->TaskOwnerIncarnation != Result.Stamp.OwnerIncarnation) return;
		auto& Current = *Component->OverlayNetwork;
		auto* CurrentModule = Component->Module.Get();
		if (!CurrentModule || CurrentModule->GetWorldEpoch() != Result.Stamp.WorldEpoch ||
			Current.BuildSerial != BuildSerial || Current.Identity.Ticket != Identity.Ticket ||
			Current.Identity.Subscription != Identity.Subscription) return;
		Current.bBuild = false;
		if (!Result.bSuccess || Result.bCanceled || !Result.CustomPayload)
		{
			Current.bNeedsSnapshot = true;
			return;
		}
		Current.Snapshot = StaticCastSharedPtr<const FVoxelOverlayAuthorityPrepared>(Result.CustomPayload);
		Current.AuthorityRoot = Current.Snapshot->Root;
		TArray<FVoxelOverlayRegionHead> Targets;
		Targets.Reserve(Current.Snapshot->Bodies.Num());
		for (const auto& Body : Current.Snapshot->Bodies)
		{
			auto Head = Body.Directory->Head;
			Head.Sequence = CurrentModule->GetRuntime()->GetOverlayRegionSequence(Head.Region);
			Targets.Add(Head);
		}
		Current.SnapshotEncoder = MakeShared<FVoxelOverlaySnapshotEncoder, ESPMode::ThreadSafe>(Current.Snapshot.ToSharedRef(),
			MoveTemp(Targets), Current.Resources.ToSharedRef(), FVoxelGenerationConsumerKey::Section(Result.Stamp.WorldEpoch,
				Result.Stamp.OwnerId, Result.Stamp.OwnerIncarnation, Result.Stamp.Section, Result.Stamp.Token));
	};
	if (WorldModule->EnqueueNetworkRepresentationTask(MoveTemp(Task)))
	{
		State.LastRetry = FPlatformTime::Seconds();
		State.bBuild = true;
		State.bNeedsSnapshot = false;
		State.Edits.Reset();
		State.Replay.Reset();
		State.ReplayBytes = 0;
		State.Acknowledged.Reset();
		State.SnapshotEncoder.Reset();
		State.DeltaEncoder.Reset();
		State.Outbound.Reset();
		State.bEncode = false;
	}
}

void UVoxelModuleNetworkComponent::CaptureOverlayCommit(const FVoxelEditBatch& Batch)
{
	auto* WorldModule = Module.Get();
	if (!OverlayNetwork || !WorldModule || !WorldModule->IsAuthority() || !OverlayNetwork->Identity.Ticket.IsValid()) return;
	auto& State = *OverlayNetwork;
	if (State.bNeedsSnapshot || Batch.Sections.IsEmpty()) return;
	if (State.Edits.Num() >= 256) { State.bNeedsSnapshot = true; return; }
	int32 Count = 0;
	for (const auto& Patch : Batch.Sections)
		if (State.Regions.Contains(FVoxelOverlayRegionCodec::RegionOf(Patch.Section))) ++Count;
	if (Count == 0) return;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	const uint64 Bytes = VoxelBoundedAllocation::ArrayBytes<TSharedPtr<const FVoxelOverlaySnapshot, ESPMode::ThreadSafe>>(Count) +
		VoxelBoundedAllocation::ArrayBytes<FVoxelOverlayRegionHead>(FMath::Min(Count, State.Regions.Num()));
	if (!State.Resources->Acquire(FVoxelGenerationConsumerKey::Section(WorldModule->GetWorldEpoch(), TaskOwnerId,
		TaskOwnerIncarnation, {}, ++NextTaskSerial), EVoxelTerrainResourcePool::Input, Bytes, Lease))
	{
		State.bNeedsSnapshot = true;
		return;
	}
	FVoxelOverlayAuthorityEditCapture Capture;
	Capture.Transaction = Batch.TransactionId;
	Capture.Cut = WorldModule->GetRuntime()->GetAuthorityCommitCut();
	Capture.ResourceLease = MoveTemp(Lease);
	Capture.Overlays.Reserve(Count);
	Capture.Heads.Reserve(FMath::Min(Count, State.Regions.Num()));
	for (const auto& Patch : Batch.Sections)
	{
		const auto Key = FVoxelOverlayRegionCodec::RegionOf(Patch.Section);
		if (!State.Regions.Contains(Key)) continue;
		const auto Overlay = WorldModule->GetRuntime()->FindCommittedOverlay(Patch.Section);
		if (!Overlay || Overlay->Revision != Patch.ToRevision) { State.bNeedsSnapshot = true; return; }
		Capture.Overlays.Add(Overlay);
		if (!Capture.Heads.ContainsByPredicate([&Key](const auto& Head) { return Head.Region == Key; }))
		{
			FVoxelOverlayRegionHead Head;
			Head.Region = Key;
			Head.Sequence = WorldModule->GetRuntime()->GetOverlayRegionSequence(Key);
			Capture.Heads.Add(Head);
		}
	}
	State.Edits.Add(MoveTemp(Capture));
}

void UVoxelModuleNetworkComponent::AdvanceOverlayProtocol(const double Now, const double Deadline)
{
	auto* WorldModule = Module.Get();
	if (!OverlayNetwork || !WorldModule) return;
	auto& State = *OverlayNetwork;
	const bool bServer = GetOwner() && GetOwner()->HasAuthority();
	if (!State.bDecode)
	{
		TSharedPtr<FVoxelOverlayInboundWork, ESPMode::ThreadSafe> Work;
		if (State.Inbound.Dequeue(Work))
		{
			FVoxelTaskRequest Task;
			Task.Kind = EVoxelTaskKind::NetworkRepresentation;
			Task.Stamp.WorldEpoch = WorldModule->GetWorldEpoch();
			Task.Stamp.OwnerId = TaskOwnerId;
			Task.Stamp.OwnerIncarnation = TaskOwnerIncarnation;
			Task.Stamp.Token = ++NextTaskSerial;
			Task.WorkClass = EVoxelWorkClass::Visible;
			Task.ServiceBand = EVoxelTerrainServiceBand::P2;
			Task.SourceStableKey = TaskOwnerId;
			Task.FirstRunnableSeconds = Now;
			Task.Execute = [Work](const TAtomic<bool>& Cancel)
			{
				FVoxelTaskResult Result;
				if (Cancel.Load()) { Result.bCanceled = true; return Result; }
				Work->Message.ResourceLease = Work->ResourceLease;
				Result.bSuccess = FVoxelOverlayRegionCodec::Decode(Work->Kind, Work->Bytes, Work->Message, Result.Error);
				if (Result.bSuccess) Result.CustomPayload = Work;
				return Result;
			};
			Task.Apply = [Weak = TWeakObjectPtr<UVoxelModuleNetworkComponent>(this), Work](FVoxelTaskResult&& Result)
			{
				auto* Component = Weak.Get();
				if (!Component || !Component->OverlayNetwork || Component->TaskOwnerId != Result.Stamp.OwnerId ||
					Component->TaskOwnerIncarnation != Result.Stamp.OwnerIncarnation || !Component->Module.IsValid() ||
					Component->Module->GetWorldEpoch() != Result.Stamp.WorldEpoch) return;
				auto& Current = *Component->OverlayNetwork;
				Current.bDecode = false;
				Current.InboundBytes -= Work->ResourceLease->Bytes();
				Work->Bytes.Empty();
				if (Result.bSuccess && !Result.bCanceled) Component->HandleOverlayMessage(Work->Kind, MoveTemp(Work->Message), Work->bServer);
				else if (!Result.bCanceled) Component->Fail(Result.Error.IsEmpty() ? TEXT("Overlay 区域消息验证失败") : Result.Error);
			};
			if (WorldModule->EnqueueNetworkRepresentationTask(MoveTemp(Task))) State.bDecode = true;
			else State.Inbound.Enqueue(MoveTemp(Work));
		}
	}
	if (FPlatformTime::Seconds() >= Deadline) return;
	if (bServer)
	{
		for (int32 Count = 0; Count < 16 && !State.Replay.IsEmpty() && FPlatformTime::Seconds() < Deadline; ++Count)
		{
			const auto Delta = State.Replay[0];
			bool bAcked = true;
			for (const auto& Head : Delta->Heads)
				if (State.Acknowledged.FindRef(Head.Region) < Head.Sequence) { bAcked = false; break; }
			if (!bAcked) break;
			State.ReplayBytes -= Delta->Payload->Bytes.GetAllocatedSize() + Delta->Heads.GetAllocatedSize();
			State.Replay.RemoveAt(0, 1, EAllowShrinking::No);
			AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [Delta] {});
		}
	}
	if (!bServer)
	{
		auto* Replica = WorldModule->GetOverlayReplica();
		if (!Replica || State.Regions.IsEmpty()) return;
		bool bComplete = true;
		bool bGap = false;
		uint64 Progress = Replica->GetProgressSerial();
		for (const auto& Key : State.Regions)
		{
			const auto RegionState = Replica->GetState(Key);
			bComplete &= RegionState == EVoxelOverlayReplicaState::Complete;
			bGap |= RegionState == EVoxelOverlayReplicaState::Gap || RegionState == EVoxelOverlayReplicaState::CatchingUp;
			FVoxelOverlayRegionHead Head;
			Replica->GetTicketHead(State.Identity.Ticket, Key, Head);
			Progress = (Progress ^ Head.Sequence ^ uint8(RegionState)) * 1099511628211ull;
		}
		if (Progress != State.LastProgress) { State.LastProgress = Progress; State.RetryCount = 0; State.LastRetry = Now; }
		if (bComplete)
		{
			if (State.RetiringTicket.IsValid())
			{
				Replica->CancelTicket(State.RetiringTicket);
				State.RetiringTicket.Invalidate();
				State.RetiringRegions.Reset();
			}
			if (Now - State.LastControl >= 0.5 && SendOverlayControl(EVoxelMessage::RegionAck, State.Regions)) State.LastControl = Now;
		}
		else if (Now - State.LastRetry >= FMath::Min(2.0, 0.25 * FMath::Pow(2.0, State.RetryCount)))
		{
			const auto Kind = State.RetryCount >= 3 || !bGap ? EVoxelMessage::ResyncRequest : EVoxelMessage::GapRequest;
			if (SendOverlayControl(Kind, State.Regions)) { State.LastRetry = Now; ++State.RetryCount; }
		}
		return;
	}
	if (State.bNeedsSnapshot && !State.bBuild) { BeginOverlaySnapshot(); return; }
	if (State.Outbound)
	{
		if (!State.Outbound->bComplete)
		{
			if (!Transfer.EnqueuePrepared(EVoxelTransferPriority::Normal, MoveTemp(State.Outbound->WireBytes),
				State.Outbound->WireCrc, State.Outbound->WireLease)) return;
		}
		else if (State.SnapshotEncoder)
		{
			State.SnapshotEncoder.Reset();
			State.Snapshot.Reset();
		}
		else
		{
			State.DeltaEncoder.Reset();
			State.EncodingDelta.Reset();
		}
		State.Outbound.Reset();
	}
	if (!State.bEncode && (State.SnapshotEncoder || State.DeltaEncoder))
	{
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
		const auto Consumer = FVoxelGenerationConsumerKey::Section(WorldModule->GetWorldEpoch(), TaskOwnerId,
			TaskOwnerIncarnation, {}, ++NextTaskSerial);
		if (!State.Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Working,
			VoxelBoundedAllocation::SharedBytes<FVoxelOverlayOutboundWork>(), Lease)) return;
		auto Work = MakeShared<FVoxelOverlayOutboundWork, ESPMode::ThreadSafe>();
		Work->ResourceLease = MoveTemp(Lease);
		const auto SnapshotEncoder = State.SnapshotEncoder;
		const auto DeltaEncoder = State.DeltaEncoder;
		FVoxelTaskRequest Task;
		Task.Kind = EVoxelTaskKind::NetworkRepresentation;
		Task.Stamp.WorldEpoch = WorldModule->GetWorldEpoch();
		Task.Stamp.OwnerId = TaskOwnerId;
		Task.Stamp.OwnerIncarnation = TaskOwnerIncarnation;
		Task.Stamp.Token = ++NextTaskSerial;
		Task.bResumable = true;
		State.EncodeStamp = Task.Stamp;
		Task.WorkClass = EVoxelWorkClass::Visible;
		Task.ServiceBand = EVoxelTerrainServiceBand::P2;
		Task.SourceStableKey = TaskOwnerId;
		Task.FirstRunnableSeconds = Now;
		Task.GetBlockReason = [Work, Resources = State.Resources]
		{
			return Work->bCapacityBlocked.Load() && Resources->Snapshot().ReleaseVersion <= Work->CapacityVersion.Load() ?
				EVoxelTaskBlockReason::Memory : EVoxelTaskBlockReason::None;
		};
		Task.Execute = [Work, SnapshotEncoder, DeltaEncoder, Resources = State.Resources, Consumer, EnvelopeSession = Session](const TAtomic<bool>& Cancel)
		{
			FVoxelTaskResult Result;
			if (Cancel.Load()) { Result.bCanceled = true; return Result; }
			Work->bCapacityBlocked.Store(false);
			if (!Work->bBodyReady)
			{
				Result.bSuccess = SnapshotEncoder ? SnapshotEncoder->Advance(Work->Message, Work->bComplete, Result.Error) :
					DeltaEncoder->Advance(Work->Message, Work->bComplete, Result.Error);
				Work->bBodyReady = Result.bSuccess;
			}
			else Result.bSuccess = true;
			if (Result.bSuccess && !Work->bComplete)
			{
				if (Work->WireBytes.IsEmpty())
				{
					const uint64 Capacity = VoxelBoundedAllocation::InitialGrowBytes<uint8>(Work->Message.Bytes.Num() + 29);
					TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Scratch;
					if ((!Work->WireLease && !Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Staged, Capacity, Work->WireLease)) ||
						!Resources->Acquire(Consumer, EVoxelTerrainResourcePool::Working, Capacity, Scratch))
					{
						Result.bSuccess = false;
						Result.Error = TEXT("CapacityBlocked: 发送 Envelope 峰值");
						Work->WireLease.Reset();
					}
					else if (!FVoxelNetworkCodec::Encode(Work->Message.Kind, EnvelopeSession, Work->Message.Bytes, Work->WireBytes) ||
						uint64(Work->WireBytes.GetAllocatedSize()) > Work->WireLease->Bytes())
					{
						Result.bSuccess = false;
						Result.Error = TEXT("发送 Envelope 编码或授予校验失败");
					}
					else
					{
						Work->WireLease->Resize(Work->WireBytes.GetAllocatedSize());
						// 私有区域正文已组成 Envelope，后台释放原编码正文。
						Work->Message.Bytes.Empty();
						Work->Message.ResourceLease.Reset();
					}
				}
				if (Result.bSuccess)
				{
					for (int32 Step = 0; Step < 16 && Work->WireCrcOffset < Work->WireBytes.Num(); ++Step)
					{
						if (Cancel.Load()) { Result.bCanceled = true; return Result; }
						const int32 Count = FMath::Min(4096, Work->WireBytes.Num() - Work->WireCrcOffset);
						Work->WireCrc = FCrc::MemCrc32(Work->WireBytes.GetData() + Work->WireCrcOffset, Count, Work->WireCrc);
						Work->WireCrcOffset += Count;
					}
					if (Work->WireCrcOffset < Work->WireBytes.Num())
					{
						Result.bContinue = true;
						return Result;
					}
				}
			}
			if (!Result.bSuccess && Result.Error.StartsWith(TEXT("CapacityBlocked:")))
			{
				Work->CapacityVersion.Store(Resources->Snapshot().ReleaseVersion);
				Work->bCapacityBlocked.Store(true);
				Result.bSuccess = Result.bContinue = true;
				Result.Error.Reset();
			}
			else if (Result.bSuccess) { Work->bCapacityBlocked.Store(false); Result.CustomPayload = Work; }
			return Result;
		};
		Task.Apply = [Weak = TWeakObjectPtr<UVoxelModuleNetworkComponent>(this), Work, SnapshotEncoder, DeltaEncoder](FVoxelTaskResult&& Result)
		{
			auto* Component = Weak.Get();
			if (!Component || !Component->OverlayNetwork || !Component->Module.IsValid() || Component->TaskOwnerId != Result.Stamp.OwnerId ||
				Component->TaskOwnerIncarnation != Result.Stamp.OwnerIncarnation || Component->Module->GetWorldEpoch() != Result.Stamp.WorldEpoch) return;
			auto& Current = *Component->OverlayNetwork;
			if (Current.SnapshotEncoder != SnapshotEncoder || Current.DeltaEncoder != DeltaEncoder) return;
			if (Current.EncodeStamp != Result.Stamp) return;
			Current.bEncode = false;
			if (Result.bSuccess && !Result.bCanceled) Current.Outbound = Work;
			else Current.bNeedsSnapshot = true;
		};
		if (WorldModule->EnqueueNetworkRepresentationTask(MoveTemp(Task))) State.bEncode = true;
		return;
	}
	if (State.bBuild || State.SnapshotEncoder || State.DeltaEncoder || State.Edits.IsEmpty() || !State.AuthorityRoot) return;
	if (State.ReplayBytes >= OverlayConnectionBytes || State.Replay.Num() >= 256) { State.bNeedsSnapshot = true; return; }
	auto Edit = MoveTemp(State.Edits[0]);
	State.Edits.RemoveAt(0, 1, EAllowShrinking::No);
	if (Edit.Cut <= State.AuthorityRoot->Identity.Cut) return;
	const auto Basis = State.AuthorityRoot;
	const auto Consumer = FVoxelGenerationConsumerKey::Section(WorldModule->GetWorldEpoch(), TaskOwnerId,
		TaskOwnerIncarnation, {}, ++NextTaskSerial);
	auto Build = MakeShared<FVoxelOverlayAuthorityDeltaBuild, ESPMode::ThreadSafe>(Basis.ToSharedRef(), MoveTemp(Edit), State.Resources.ToSharedRef(), Consumer);
	FVoxelTaskRequest Task;
	Task.Kind = EVoxelTaskKind::NetworkRepresentation;
	Task.Stamp.WorldEpoch = WorldModule->GetWorldEpoch();
	Task.Stamp.OwnerId = TaskOwnerId;
	Task.Stamp.OwnerIncarnation = TaskOwnerIncarnation;
	Task.Stamp.Token = ++NextTaskSerial;
	Task.WorkClass = EVoxelWorkClass::Visible;
	State.BuildStamp = Task.Stamp;
	Task.ServiceBand = EVoxelTerrainServiceBand::P2;
	Task.SourceStableKey = TaskOwnerId;
	Task.FirstRunnableSeconds = Now;
	Task.bResumable = true;
	Task.GetBlockReason = [Build, Resources = State.Resources]
	{
		return Build->IsCapacityBlocked() && Resources->Snapshot().ReleaseVersion <= Build->GetCapacityVersion() ?
			EVoxelTaskBlockReason::Memory : EVoxelTaskBlockReason::None;
	};
	Task.Execute = [Build](const TAtomic<bool>& Cancel) { return Build->Advance(Cancel); };
	Task.Apply = [Weak = TWeakObjectPtr<UVoxelModuleNetworkComponent>(this), Basis](FVoxelTaskResult&& Result)
	{
		auto* Component = Weak.Get();
		if (!Component || !Component->OverlayNetwork || Component->TaskOwnerId != Result.Stamp.OwnerId ||
			Component->TaskOwnerIncarnation != Result.Stamp.OwnerIncarnation) return;
		auto& Current = *Component->OverlayNetwork;
		if (Current.AuthorityRoot != Basis) return;
		Current.bBuild = false;
		if (!Result.bSuccess || Result.bCanceled || !Result.CustomPayload) { Current.bNeedsSnapshot = true; return; }
		const auto Delta = StaticCastSharedPtr<const FVoxelOverlayAuthorityDelta>(Result.CustomPayload);
		const uint64 Bytes = Delta->Payload->Bytes.GetAllocatedSize() + Delta->Heads.GetAllocatedSize();
		if (Current.ReplayBytes + Bytes > OverlayConnectionBytes) { Current.bNeedsSnapshot = true; return; }
		Current.AuthorityRoot = Delta->Root;
		Current.Replay.Add(Delta);
		Current.ReplayBytes += Bytes;
		Current.EncodingDelta = Delta;
		Current.DeltaEncoder = MakeShared<FVoxelOverlayDeltaEncoder, ESPMode::ThreadSafe>(Delta.ToSharedRef(), Current.Resources.ToSharedRef(),
			FVoxelGenerationConsumerKey::Section(Result.Stamp.WorldEpoch, Result.Stamp.OwnerId, Result.Stamp.OwnerIncarnation, {}, Result.Stamp.Token));
	};
	if (WorldModule->EnqueueNetworkRepresentationTask(MoveTemp(Task))) State.bBuild = true;
	else State.bNeedsSnapshot = true;
}

#include "Voxel/Network/VoxelModuleNetworkComponent.h"

#include "VoxelModuleNetworkOverlayState.h"
#include "Voxel/Network/VoxelNetworkCodec.h"
#include "Voxel/Generation/VoxelManifestCodec.h"
#include "Voxel/Runtime/VoxelWorldRuntime.h"
#include "Voxel/Task/VoxelBoundedAllocation.h"
#include "Voxel/Generation/VoxelGenerationAllocation.h"
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
	bool SameControlIdentity(const FVoxelOverlayAuthorityIdentity& A, const FVoxelOverlayAuthorityIdentity& B)
	{
		return A.WorldEpoch == B.WorldEpoch && A.Manifest == B.Manifest && A.DirectoryEpoch == B.DirectoryEpoch &&
			A.Subscription == B.Subscription && A.Ticket == B.Ticket && A.Cut == B.Cut;
	}
	bool SameSubscriptionIdentity(const FVoxelOverlayAuthorityIdentity& A, const FVoxelOverlayAuthorityIdentity& B)
	{
		return A.WorldEpoch == B.WorldEpoch && A.Manifest == B.Manifest && A.DirectoryEpoch == B.DirectoryEpoch &&
			A.Subscription == B.Subscription;
	}
	bool SameControlLimits(const FVoxelTerrainResourceLimits& A, const FVoxelTerrainResourceLimits& B)
	{
		return A.InputBytes == B.InputBytes && A.WorkingBytes == B.WorkingBytes && A.StagedBytes == B.StagedBytes &&
			A.ResidentBytes == B.ResidentBytes && A.OverlapBytes == B.OverlapBytes;
	}
	FVoxelOverlayRegionMessage ControlMessage(const FVoxelOverlayAuthorityIdentity& Identity)
	{
		FVoxelOverlayRegionMessage Message;
		Message.WorldEpoch = Identity.WorldEpoch;
		Message.ManifestIdentity = Identity.Manifest;
		Message.DirectoryEpoch = Identity.DirectoryEpoch;
		Message.SubscriptionId = Identity.Subscription;
		Message.CoverageTicket = Identity.Ticket;
		Message.AuthorityCut = Identity.Cut;
		return Message;
	}
	bool ResumeControl(FVoxelTerrainResources& Resources, FVoxelOverlayControlAdmission& Attempt)
	{
		if (Attempt.Admission == EVoxelTerrainResourceAdmission::CapacityUnachievable ||
			Attempt.Admission == EVoxelTerrainResourceAdmission::InvalidRequest) return false;
		uint64 Current = 0;
		if (!Attempt.Consumer.IsValid())
		{
			Attempt.Admission = EVoxelTerrainResourceAdmission::InvalidRequest;
			return false;
		}
		if (!Resources.TryGetReleaseVersionExcludingOwner(Attempt.Consumer, Current)) return false;
		if (Attempt.Admission == EVoxelTerrainResourceAdmission::CapacityBlocked && Current <= Attempt.CapacityVersion) return false;
		// 在实际 grant 前采样；本尝试失败时的退款不能成为自己的恢复事件。
		Attempt.CapacityVersion = Current;
		return true;
	}
	EVoxelTerrainResourceAdmission OverlayAdmissionFromError(const FString& Error)
	{
		return Error.StartsWith(TEXT("CapacityBlocked:")) ? EVoxelTerrainResourceAdmission::CapacityBlocked :
			Error.StartsWith(TEXT("CapacityUnachievable:")) || Error.StartsWith(TEXT("Oversized:")) ?
			EVoxelTerrainResourceAdmission::CapacityUnachievable : EVoxelTerrainResourceAdmission::InvalidRequest;
	}
	bool GrantFactoryInput(FVoxelTerrainResources& Resources, FVoxelOverlayControlAdmission& Attempt,
		const uint64 Bytes, TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& OutLease)
	{
		if (!ResumeControl(Resources, Attempt)) return false;
		FVoxelGenerationRequiredCapacity Required;
		FString Error;
		if (!FVoxelGenerationRequiredSourceScope::Collect(Resources, nullptr, Required, Error))
		{
			Attempt.Admission = EVoxelTerrainResourceAdmission::InvalidRequest;
			return false;
		}
		Attempt.Admission = Resources.TryAcquire(Attempt.Consumer, EVoxelTerrainResourcePool::Input, Bytes, OutLease, 0,
			Required.Bytes[int32(EVoxelTerrainResourcePool::Input)], Required.LeaseCount);
		return Attempt.Admission == EVoxelTerrainResourceAdmission::Granted;
	}
	void RecordSnapshotFactoryFailure(FVoxelOverlayNetworkState& State)
	{
		const auto& Attempt = State.SnapshotFactoryAdmission;
		if (Attempt.Admission != EVoxelTerrainResourceAdmission::CapacityUnachievable && Attempt.Admission != EVoxelTerrainResourceAdmission::InvalidRequest) return;
		State.SnapshotTerminalIdentity = Attempt.Identity;
		State.SnapshotTerminalLimits = Attempt.Limits;
		State.SnapshotTerminalConsumer = Attempt.Consumer;
		State.bSnapshotTerminalFailure = true;
		State.bNeedsSnapshot = true;
	}
	void ClearEncoderFactory(FVoxelOverlayNetworkState& State)
	{
		State.EncoderFactoryHeads.Empty();
		State.EncoderFactoryHeadLease.Reset();
		State.EncoderFactoryAdmission = FVoxelOverlayControlAdmission();
		State.bEncoderDeltaCommitPending = false;
	}
	void SetEncoderFactory(FVoxelOverlayNetworkState& State, const FVoxelGenerationConsumerKey& Consumer,
		const FVoxelOverlayAuthorityIdentity& Identity, const EVoxelMessage Kind)
	{
		ClearEncoderFactory(State);
		State.EncoderFactoryAdmission.Consumer = Consumer;
		State.EncoderFactoryAdmission.Identity = Identity;
		State.EncoderFactoryAdmission.Limits = State.Resources->Snapshot().Limits;
		State.EncoderFactoryAdmission.Kind = Kind;
	}
	bool CreatePendingOverlayEncoder(FVoxelOverlayNetworkState& State, UVoxelModule& Module)
	{
		if (State.SnapshotEncoder || State.DeltaEncoder) return true;
		if (!State.Snapshot && !State.EncodingDelta) return true;
		auto& Attempt = State.EncoderFactoryAdmission;
		if ((Attempt.Admission == EVoxelTerrainResourceAdmission::CapacityUnachievable || Attempt.Admission == EVoxelTerrainResourceAdmission::InvalidRequest) &&
			(Module.GetRuntime()->GetAuthorityCommitCut() != Attempt.Identity.Cut || !SameControlLimits(Attempt.Limits, State.Resources->Snapshot().Limits)))
		{
			State.Snapshot.Reset();
			State.EncodingDelta.Reset();
			ClearEncoderFactory(State);
			State.bNeedsSnapshot = true;
			return false;
		}
		if (!ResumeControl(*State.Resources, Attempt)) return false;
		uint64 BeforeAdmission = Attempt.CapacityVersion;
		FVoxelGenerationRequiredSourceScope StateSource(State.Resources.Get(), State.ResourceLease.Get());
		struct FInput { const FVoxelOverlayNetworkState& State; } Input{State};
		FVoxelGenerationRequiredSourceScope InputSource(State.Resources.Get(), &Input,
			[](const void* Context, FVoxelGenerationRequiredSourceScope::FVisit Visit)
			{
				const auto& State = static_cast<const FInput*>(Context)->State;
				if (State.AuthorityRoot) FVoxelOverlayAuthorityDeltaBuild::VisitRootSources(*State.AuthorityRoot, *State.Resources, Visit);
				if (State.Snapshot) FVoxelOverlaySnapshotEncoder::VisitSnapshotSources(*State.Snapshot, *State.Resources, Visit);
				if (State.EncodingDelta) FVoxelOverlayDeltaEncoder::VisitDeltaSources(*State.EncodingDelta, *State.Resources, Visit);
				if (State.EncoderFactoryHeadLease) FVoxelGenerationRequiredSourceScope::VisitLease(State.EncoderFactoryHeadLease.Get(), Visit);
			});
		FString Error;
		if (State.Snapshot)
		{
			if (!State.EncoderFactoryHeadLease)
			{
				const int32 Count = State.Snapshot->Bodies.Num();
				if (!GrantFactoryInput(*State.Resources, Attempt,
					VoxelGenerationAllocation::Private::ArrayReserveBytes<FVoxelOverlayRegionHead, FDefaultAllocator>(Count), State.EncoderFactoryHeadLease)) return false;
				State.EncoderFactoryHeads.Reserve(Count);
				if (VoxelGenerationAllocation::AllocatedBytes(State.EncoderFactoryHeads) > State.EncoderFactoryHeadLease->Bytes())
				{
					Attempt.Admission = EVoxelTerrainResourceAdmission::InvalidRequest;
					return false;
				}
				for (const auto& Body : State.Snapshot->Bodies)
				{
					auto Head = Body.Directory->Head;
					Head.Sequence = Module.GetRuntime()->GetOverlayRegionSequence(Head.Region);
					State.EncoderFactoryHeads.Add(Head);
				}
				State.EncoderFactoryHeadLease->MarkPublished();
			}
			BeforeAdmission = State.Resources->ReleaseVersionExcludingOwner(Attempt.Consumer);
			Attempt.Admission = FVoxelOverlaySnapshotEncoder::Create(State.Snapshot.ToSharedRef(), MoveTemp(State.EncoderFactoryHeads),
				State.EncoderFactoryHeadLease, State.Resources.ToSharedRef(), Attempt.Consumer, State.SnapshotEncoder, Error);
			if (Attempt.Admission == EVoxelTerrainResourceAdmission::Granted) State.AuthorityRoot = State.Snapshot->Root;
		}
		else
		{
			BeforeAdmission = State.Resources->ReleaseVersionExcludingOwner(Attempt.Consumer);
			Attempt.Admission = FVoxelOverlayDeltaEncoder::Create(State.EncodingDelta.ToSharedRef(), State.Resources.ToSharedRef(),
				Attempt.Consumer, State.DeltaEncoder, Error);
			if (Attempt.Admission == EVoxelTerrainResourceAdmission::Granted && State.bEncoderDeltaCommitPending)
			{
				State.AuthorityRoot = State.EncodingDelta->Root;
				State.Replay.Add(State.EncodingDelta);
				State.ReplayBytes += State.EncodingDelta->Payload->Bytes.GetAllocatedSize() + State.EncodingDelta->Heads.GetAllocatedSize();
			}
		}
		if (Attempt.Admission != EVoxelTerrainResourceAdmission::Granted)
		{
			Attempt.CapacityVersion = BeforeAdmission;
			return false;
		}
		ClearEncoderFactory(State);
		State.OutboundFactoryAdmission = FVoxelOverlayControlAdmission();
		return true;
	}

	bool GrantControlScratch(FVoxelTerrainResources& Resources, FVoxelOverlayControlAdmission& Attempt,
		const uint64 Bytes, TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe>& OutLease)
	{
		if (!ResumeControl(Resources, Attempt)) return false;
		FVoxelGenerationRequiredCapacity Required;
		FString Error;
		if (!FVoxelGenerationRequiredSourceScope::Collect(Resources, nullptr, Required, Error))
		{
			Attempt.Admission = EVoxelTerrainResourceAdmission::InvalidRequest;
			return false;
		}
		const uint64 WorkingLimit = Resources.Snapshot().Limits.WorkingBytes;
		const uint64 RetainedWorking = Required.Bytes[int32(EVoxelTerrainResourcePool::Working)];
		if (RetainedWorking > WorkingLimit || Bytes > WorkingLimit - RetainedWorking)
		{
			Attempt.Admission = EVoxelTerrainResourceAdmission::CapacityUnachievable;
			return false;
		}
		Attempt.Admission = Resources.TryAcquire(Attempt.Consumer, EVoxelTerrainResourcePool::Working, Bytes, OutLease, 0,
			Required.Bytes[int32(EVoxelTerrainResourcePool::Input)], Required.LeaseCount);
		return Attempt.Admission == EVoxelTerrainResourceAdmission::Granted;
	}
}

bool UVoxelModuleNetworkComponent::EnsureTransferResources(FString& OutError)
{
	auto* WorldModule = Module.Get();
	if (!WorldModule)
	{
		OutError = TEXT("InvalidResource: original network module is unavailable");
		return false;
	}
	TSharedPtr<FVoxelTerrainResources, ESPMode::ThreadSafe> Resources;
	FVoxelGenerationConsumerKey Control;
	if (!WorldModule->EnsureTerrainFactBootstrap(Resources, Control, OutError)) return false;
	if (!NetworkTransferConsumer.IsValid())
	{
		if (NextTaskSerial == MAX_uint64)
		{
			OutError = TEXT("Oversized: original transfer consumer identity");
			return false;
		}
		NetworkTransferConsumer = FVoxelGenerationConsumerKey::Section(Control.WorldEpoch, TaskOwnerId,
			TaskOwnerIncarnation, {}, ++NextTaskSerial);
	}
	if (NetworkTransferConsumer.WorldEpoch != Control.WorldEpoch || NetworkTransferConsumer.OwnerId != TaskOwnerId ||
		NetworkTransferConsumer.OwnerIncarnation != TaskOwnerIncarnation ||
		(bNetworkTransferBound && (Transfer.GetResources() != Resources || Transfer.GetConsumer() != NetworkTransferConsumer)))
	{
		OutError = TEXT("InvalidResource: original transfer domain changed");
		return false;
	}
	if (bNetworkTransferBound)
	{
		OutError.Reset();
		return true;
	}
	uint64 BeforeAdmission = 0;
	if (!Resources->TryGetReleaseVersionExcludingOwner(NetworkTransferConsumer, BeforeAdmission) ||
		(bNetworkStateWaitingCapacity && NetworkStateCapacityConsumer == NetworkTransferConsumer &&
			BeforeAdmission <= NetworkStateCapacityVersion))
	{
		OutError = TEXT("CapacityBlocked: original transfer binding awaits release");
		return false;
	}
	FVoxelGenerationRequiredSourceScope StateSource(Resources.Get(), OverlayNetwork ? OverlayNetwork->ResourceLease.Get() : nullptr);
	if (!Transfer.BindResources(Resources.ToSharedRef(), NetworkTransferConsumer))
	{
		const auto Admission = Transfer.GetLastResourceAdmission();
		bNetworkStateWaitingCapacity = Admission == EVoxelTerrainResourceAdmission::CapacityBlocked;
		NetworkStateCapacityConsumer = NetworkTransferConsumer;
		NetworkStateCapacityVersion = BeforeAdmission;
		OutError = bNetworkStateWaitingCapacity ? TEXT("CapacityBlocked: original transfer directory") :
			Admission == EVoxelTerrainResourceAdmission::CapacityUnachievable ? TEXT("Oversized: original transfer directory") :
			TEXT("InvalidResource: original transfer binding");
		return false;
	}
	if (NetworkStateCapacityConsumer == NetworkTransferConsumer) bNetworkStateWaitingCapacity = false;
	bNetworkTransferBound = true;
	OutError.Reset();
	return true;
}

bool UVoxelModuleNetworkComponent::EnsureNetworkState(FString& OutError)
{
	auto* WorldModule = Module.Get();
	if (!WorldModule || !WorldModule->IsReady())
	{
		OutError = TEXT("InvalidResource: original network world is unavailable");
		return false;
	}
	const auto Resources = WorldModule->GetTerrainFactResources();
	if (!Resources)
	{
		OutError = TEXT("InvalidResource: original network fact ledger is unavailable");
		return false;
	}
	if (!EnsureTransferResources(OutError)) return false;
	if (!NetworkStateConsumer.IsValid())
	{
		if (NextTaskSerial == MAX_uint64)
		{
			OutError = TEXT("Oversized: original network state identity");
			return false;
		}
		NetworkStateConsumer = FVoxelGenerationConsumerKey::Section(WorldModule->GetWorldEpoch(), TaskOwnerId,
			TaskOwnerIncarnation, {}, ++NextTaskSerial);
	}
	if (NetworkStateConsumer.WorldEpoch != WorldModule->GetWorldEpoch() || NetworkStateConsumer.OwnerId != TaskOwnerId ||
		NetworkStateConsumer.OwnerIncarnation != TaskOwnerIncarnation ||
		(OverlayNetwork && OverlayNetwork->Resources != Resources))
	{
		OutError = TEXT("InvalidResource: original network state domain changed");
		return false;
	}
	if (bNetworkStateWaitingCapacity)
	{
		uint64 Current = 0;
		if (!Resources->TryGetReleaseVersionExcludingOwner(NetworkStateCapacityConsumer, Current) || Current <= NetworkStateCapacityVersion)
		{
			OutError = TEXT("CapacityBlocked: original network state awaits release");
			return false;
		}
		bNetworkStateWaitingCapacity = false;
	}
	if (!OverlayNetwork)
	{
		const uint64 BeforeAdmission = Resources->ReleaseVersionExcludingOwner(NetworkStateConsumer);
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Scratch;
		TMap<FIntVector, uint64> EmptyAcknowledged;
		const uint64 Bytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelOverlayNetworkState>() +
			VoxelGenerationAllocation::Private::ArrayReserveBytes<FIntVector, FDefaultAllocator>(FVoxelOverlayRegionCodec::MaximumRegions) * 2 +
			VoxelGenerationAllocation::Private::ArrayReserveBytes<FVoxelOverlayAuthorityEditCapture, FDefaultAllocator>(256) +
			VoxelGenerationAllocation::Private::ArrayReserveBytes<TSharedPtr<const FVoxelOverlayAuthorityDelta, ESPMode::ThreadSafe>, FDefaultAllocator>(256) +
			VoxelGenerationAllocation::Private::SetReserveBytes<TPair<FIntVector, uint64>>(EmptyAcknowledged, FVoxelOverlayRegionCodec::MaximumRegions);
		FVoxelGenerationRequiredSourceScope DirectorySource(Resources.Get(), Transfer.GetMetadataLease().Get());
		FVoxelGenerationRequiredCapacity Required;
		if (!FVoxelGenerationRequiredSourceScope::Collect(*Resources, nullptr, Required, OutError)) return false;
		auto Admission = Resources->TryAcquire(NetworkStateConsumer, EVoxelTerrainResourcePool::Input, Bytes, Lease, 0,
			Required.Bytes[int32(EVoxelTerrainResourcePool::Input)], Required.LeaseCount);
		FVoxelGenerationRequiredSourceScope CandidateSource(Resources.Get(), Lease.Get());
		if (Admission == EVoxelTerrainResourceAdmission::Granted)
		{
			if (!FVoxelGenerationRequiredSourceScope::Collect(*Resources, nullptr, Required, OutError)) return false;
			Admission = Resources->TryAcquire(NetworkStateConsumer, EVoxelTerrainResourcePool::Working,
				VoxelGenerationAllocation::Private::ArrayReserveBytes<uint8, FDefaultAllocator>(FVoxelManifestCodec::MaxBytes), Scratch, 0,
				Required.Bytes[int32(EVoxelTerrainResourcePool::Input)], Required.LeaseCount);
		}
		if (Admission != EVoxelTerrainResourceAdmission::Granted)
		{
			bNetworkStateWaitingCapacity = Admission == EVoxelTerrainResourceAdmission::CapacityBlocked;
			NetworkStateCapacityConsumer = NetworkStateConsumer;
			NetworkStateCapacityVersion = BeforeAdmission;
			OutError = bNetworkStateWaitingCapacity ? TEXT("CapacityBlocked: original network state backing") :
				Admission == EVoxelTerrainResourceAdmission::CapacityUnachievable ? TEXT("Oversized: original network state backing") :
				TEXT("InvalidResource: original network state consumer");
			return false;
		}
		auto State = VoxelBoundedAllocation::MakeWorkerShared<FVoxelOverlayNetworkState>(Lease);
		State->Resources = Resources;
		State->ResourceLease = MoveTemp(Lease);
		State->Regions.Reserve(FVoxelOverlayRegionCodec::MaximumRegions);
		State->RetiringRegions.Reserve(FVoxelOverlayRegionCodec::MaximumRegions);
		State->Edits.Reserve(256);
		State->Replay.Reserve(256);
		State->Acknowledged.Reserve(FVoxelOverlayRegionCodec::MaximumRegions);
		const uint64 ActualBytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelOverlayNetworkState>() +
			VoxelGenerationAllocation::AllocatedBytes(State->Regions) + VoxelGenerationAllocation::AllocatedBytes(State->RetiringRegions) +
			VoxelGenerationAllocation::AllocatedBytes(State->Edits) + VoxelGenerationAllocation::AllocatedBytes(State->Replay) +
			VoxelGenerationAllocation::AllocatedBytes(State->Acknowledged);
		if (ActualBytes > Bytes || State->ResourceLease->TryResize(ActualBytes) != EVoxelTerrainResourceAdmission::Granted)
		{
			OutError = TEXT("InvalidResource: original network state reserve contract");
			return false;
		}
		State->Identity.WorldEpoch = WorldModule->IsAuthority() ? WorldModule->GetWorldEpoch() :
			(WorldModule->GetOverlayReplica() ? WorldModule->GetOverlayReplica()->GetAuthorityEpoch() : 0);
		State->Identity.DirectoryEpoch = Session;
		if (!FVoxelOverlayRegionCodec::ManifestHash(WorldModule->GetManifest(), State->Identity.Manifest))
		{
			OutError = TEXT("InvalidResource: original network manifest identity");
			return false;
		}
		State->Identity.Subscription = FGuid::NewGuid();
		State->ResourceLease->MarkPublished();
		OverlayNetwork = MoveTemp(State);
	}
	OutError.Reset();
	return true;
}

FVoxelTerrainNetworkStatus UVoxelModuleNetworkComponent::GetTerrainNetworkStatus() const
{
	FVoxelTerrainNetworkStatus Status;
	if (!OverlayNetwork) return Status;
	const auto& State = *OverlayNetwork;
	Status.bActive = true;
	Status.Phase = State.AdvancePhase;
	Status.Regions = State.Regions.Num();
	Status.BuildSerial = State.BuildSerial;
	Status.PacketBytes = State.PacketBytes;
	Status.EnvelopeBytes = State.EnvelopeBytes;
	Status.InboundBytes = State.InboundBytes;
	Status.bSubscriptionPending = State.bSubscriptionAttempt;
	Status.bBuilding = State.bBuild || State.SnapshotBuildHead.IsValid();
	Status.bEncoding = State.bEncode || State.SnapshotEncoder.IsValid() || State.DeltaEncoder.IsValid();
	Status.bDecoding = State.bDecode || State.bEnvelopeDecode;
	Status.bNeedsSnapshot = State.bNeedsSnapshot;
	Status.bSnapshotTerminalFailure = State.bSnapshotTerminalFailure;
	Status.SubscriptionAdmission = int32(State.SubscriptionAdmission.Admission);
	Status.SnapshotAdmission = int32(State.SnapshotFactoryAdmission.Admission);
	Status.EncoderAdmission = int32(State.EncoderFactoryAdmission.Admission);
	Status.OutboundAdmission = int32(State.OutboundFactoryAdmission.Admission);
	Status.AuthorityEpoch = State.Identity.WorldEpoch;
	Status.DirectoryEpoch = State.Identity.DirectoryEpoch;
	Status.Session = Session;
	Status.Subscription = State.Identity.Subscription;
	Status.Ticket = State.Identity.Ticket;
	return Status;
}

void UVoxelModuleNetworkComponent::AdvanceTerrainNetwork(FVoxelTerrainFrameContext& Frame)
{
	auto* WorldModule = Module.Get();
	if (!WorldModule || !WorldModule->IsReady() || bRejected || Frame.GetWorldEpoch() != WorldModule->GetWorldEpoch()) return;
	FString InitializationError;
	if (!EnsureNetworkState(InitializationError))
	{
		if (!InitializationError.StartsWith(TEXT("CapacityBlocked"))) Fail(InitializationError);
		return;
	}
	ReserveRepresentationTables(); // 扩容失败保留原容量，控制流和旧请求继续服务。
	const double Now = FPlatformTime::Seconds();
	const double Deadline = Frame.Deadline(0.1);
	const auto StateOwner = OverlayNetwork;
	// 每阶段先交接游标，预算耗尽后下帧从未服务阶段继续。
	for (int32 Count = 0; Count < 9 && FPlatformTime::Seconds() < Deadline; ++Count)
	{
		const int32 Phase = StateOwner->AdvancePhase;
		StateOwner->AdvancePhase = (Phase + 1) % 9;
		switch (Phase)
		{
		case 0:
			ApplyDecodedEnvelope(false);
			break;
		case 1:
			Transfer.Tick(Now, [this](TArray<uint8>&& Fragment)
			{
				FVoxelRPCPacket Packet;
				Packet.Bytes = MoveTemp(Fragment);
				if (GetOwner() && GetOwner()->HasAuthority()) ClientReceive(Packet);
				else ServerReceive(Packet);
			}, Deadline);
			if (OverlayNetwork != StateOwner || bRejected) return;
			if (Transfer.ConsumeReceiveGap())
			{
				DeferIngressResync(NetworkTransferConsumer, 0, GetOwner() && GetOwner()->HasAuthority(), false);
			}
			break;
		case 2:
			for (int32 PacketCount = 0; PacketCount < 4 && FPlatformTime::Seconds() < Deadline; ++PacketCount)
			{
				if (!StateOwner->PacketHead.IsSet())
				{
					FVoxelOverlayQueuedPacket Packet;
					if (!StateOwner->Packets.Dequeue(Packet)) break;
					StateOwner->PacketHead.Emplace(MoveTemp(Packet));
				}
				if (!ProcessPacket(StateOwner->PacketHead.GetValue())) break;
				if (OverlayNetwork != StateOwner || bRejected) return;
				StateOwner->PacketBytes -= StateOwner->PacketHead->ResourceLease->Bytes();
				StateOwner->PacketHead.Reset();
				StateOwner->bPacketHeadWaitingCapacity = false;
			}
			break;
		case 3:
			AdvanceEnvelopeDecode(Now);
			break;
		case 4:
			if (GetOwner() && !GetOwner()->HasAuthority() && !bWelcomed && !bPendingReady && Now - LastHello >= 1.0)
				SendHello(Now);
			break;
		case 5:
			if (bReady && GetOwner()) AdvancePendingRepresentations(Deadline);
			break;
		case 6:
			if (bReady && GetOwner())
			{
				if (!GetOwner()->HasAuthority()) RefreshOverlaySubscription(Now);
				else AdvanceRepresentationSubscriptions(Now, Deadline);
			}
			break;
		case 7:
			if (bReady && GetOwner() && GetOwner()->HasAuthority() && Now - LastInterest >= 0.25) RefreshInterest(Now);
			break;
		case 8:
			if (bReady && GetOwner()) AdvanceOverlayProtocol(Now, Deadline);
			break;
		}
		if (OverlayNetwork != StateOwner || bRejected || !WorldModule->IsReady() || Frame.GetWorldEpoch() != WorldModule->GetWorldEpoch()) return;
	}
}

bool UVoxelModuleNetworkComponent::SendOverlayControl(const EVoxelMessage Kind, TConstArrayView<FIntVector> Regions)
{
	if (!OverlayNetwork || Regions.IsEmpty() || Regions.Num() > FVoxelOverlayRegionCodec::MaximumRegions) return false;
	const auto* WorldModule = Module.Get();
	if (!WorldModule) return false;
	auto& State = *OverlayNetwork;
	const bool bSubscription = Kind == EVoxelMessage::SubscribeRegion && State.bSubscriptionAttempt;
	auto& Attempt = bSubscription ? State.SubscriptionAdmission : State.ControlAdmission;
	const auto Identity = bSubscription ? Attempt.Identity : State.Identity;
	if (!Identity.Ticket.IsValid()) return false;
	const auto Limits = State.Resources->Snapshot().Limits;
	if (!bSubscription && (!Attempt.Consumer.IsValid() || Attempt.Kind != Kind ||
		!SameControlIdentity(Attempt.Identity, Identity) || !SameControlLimits(Attempt.Limits, Limits)))
	{
		Attempt = FVoxelOverlayControlAdmission();
		Attempt.Identity = Identity;
		Attempt.Kind = Kind;
		Attempt.Limits = Limits;
		if (NextTaskSerial == MAX_uint64)
		{
			Attempt.Admission = EVoxelTerrainResourceAdmission::CapacityUnachievable;
			return false;
		}
		Attempt.Consumer = FVoxelGenerationConsumerKey::Section(WorldModule->GetWorldEpoch(),
			TaskOwnerId, TaskOwnerIncarnation, Regions[0], ++NextTaskSerial);
	}
	if (!ResumeControl(*State.Resources, Attempt)) return false;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Scratch;
	auto Message = ControlMessage(Identity);
	Message.Region = Regions[0];
	int32 EncodedBytes = 0;
	if (!FVoxelOverlayRegionCodec::EncodedByteCount(Message, EncodedBytes, Regions.Num()))
	{
		Attempt.Admission = EVoxelTerrainResourceAdmission::InvalidRequest;
		return false;
	}
	const uint64 Bytes = VoxelGenerationAllocation::Private::ArrayReserveBytes<FVoxelOverlayRegionHead, FDefaultAllocator>(Regions.Num()) +
		VoxelGenerationAllocation::Private::ArrayReserveBytes<uint8, FDefaultAllocator>(EncodedBytes);
	FVoxelGenerationRequiredSourceScope StateSource(State.Resources.Get(), State.ResourceLease.Get());
	FVoxelGenerationRequiredSourceScope TransferSource(State.Resources.Get(), Transfer.GetMetadataLease().Get());
	if (!GrantControlScratch(*State.Resources, Attempt, Bytes, Scratch)) return false;
	FVoxelGenerationRequiredSourceScope ScratchSource(State.Resources.Get(), Scratch.Get());
	Message.Heads.Reserve(Regions.Num());
	for (const auto& Key : Regions)
	{
		FVoxelOverlayRegionHead Head;
		Head.Region = Key;
		if (const auto* Replica = WorldModule->GetOverlayReplica()) Replica->GetTicketHead(Identity.Ticket, Key, Head);
		Message.Heads.Add(Head);
	}
	TArray<uint8> Payload;
	FString Error;
	if (!FVoxelOverlayRegionCodec::Encode(Kind, Message, Payload) ||
		VoxelGenerationAllocation::AllocatedBytes(Message.Heads) + VoxelGenerationAllocation::AllocatedBytes(Payload) != Bytes)
	{
		Attempt.Admission = EVoxelTerrainResourceAdmission::InvalidRequest;
		return false;
	}
	if (!Send(Kind, Payload, EVoxelTransferPriority::Normal, &Attempt.Consumer, &Error))
	{
		Attempt.Admission = Error.StartsWith(TEXT("CapacityBlocked:")) || Error.StartsWith(TEXT("PlanningPending:")) ?
			EVoxelTerrainResourceAdmission::CapacityBlocked :
			Error.StartsWith(TEXT("Oversized:")) || Error.StartsWith(TEXT("CapacityUnachievable:")) ?
			EVoxelTerrainResourceAdmission::CapacityUnachievable : EVoxelTerrainResourceAdmission::InvalidRequest;
		return false;
	}
	Attempt = FVoxelOverlayControlAdmission();
	return true;
}

void UVoxelModuleNetworkComponent::RefreshOverlaySubscription(const double Now)
{
	auto* WorldModule = Module.Get();
	if (!OverlayNetwork || !WorldModule || !WorldModule->GetOverlayReplica()) return;
	auto& State = *OverlayNetwork;
	auto* Replica = WorldModule->GetOverlayReplica();
	auto Identity = State.Identity;
	Identity.WorldEpoch = Replica->GetAuthorityEpoch();
	Identity.DirectoryEpoch = Replica->GetDirectoryEpoch();
	Identity.Manifest = Replica->GetManifestIdentity();
	const auto Current = WorldModule->GetCurrentInterestSnapshot();
	const auto Desired = WorldModule->GetDesiredInterestSnapshot();
	const auto Limits = State.Resources->Snapshot().Limits;
	const auto CancelAttempt = [&State, Replica]
	{
		if (State.bSubscriptionExpected) Replica->CancelTicket(State.SubscriptionAdmission.Identity.Ticket);
		State.SubscriptionAdmission = FVoxelOverlayControlAdmission();
		State.bSubscriptionAttempt = State.bSubscriptionExpected = false;
	};
	const bool bSamePublishedIdentity = SameSubscriptionIdentity(Identity, State.Identity);
	if (Current->DemandRevision == State.SeenCurrentDemandRevision && Desired->DemandRevision == State.SeenDesiredDemandRevision &&
		Current->ResidencyRevision == State.SeenCurrentResidencyRevision && Desired->ResidencyRevision == State.SeenDesiredResidencyRevision &&
		!State.Regions.IsEmpty() && bSamePublishedIdentity)
	{
		if (State.bSubscriptionAttempt) CancelAttempt();
		return;
	}
	// DataSections 与 Fine Current/Pending 窗口仅沿 Demand/Residency 原前驱更新。
	// 视觉分区或 priority-only 发布不能绕过同一范围的容量等待。
	if (!State.bSubscriptionAttempt || State.SubscriptionCurrentDemandRevision != Current->DemandRevision ||
		State.SubscriptionDesiredDemandRevision != Desired->DemandRevision ||
		State.SubscriptionCurrentResidencyRevision != Current->ResidencyRevision ||
		State.SubscriptionDesiredResidencyRevision != Desired->ResidencyRevision ||
		!SameSubscriptionIdentity(State.SubscriptionAdmission.Identity, Identity) ||
		!SameControlLimits(State.SubscriptionAdmission.Limits, Limits))
	{
		CancelAttempt();
		State.bSubscriptionAttempt = true;
		State.SubscriptionCurrentDemandRevision = Current->DemandRevision;
		State.SubscriptionDesiredDemandRevision = Desired->DemandRevision;
		State.SubscriptionCurrentResidencyRevision = Current->ResidencyRevision;
		State.SubscriptionDesiredResidencyRevision = Desired->ResidencyRevision;
		State.SubscriptionAdmission.Identity = Identity;
		State.SubscriptionAdmission.Identity.Ticket.Invalidate();
		State.SubscriptionAdmission.Kind = EVoxelMessage::SubscribeRegion;
		State.SubscriptionAdmission.Limits = Limits;
		if (NextTaskSerial == MAX_uint64)
		{
			State.SubscriptionAdmission.Admission = EVoxelTerrainResourceAdmission::CapacityUnachievable;
			return;
		}
		State.SubscriptionAdmission.Consumer = FVoxelGenerationConsumerKey::Section(WorldModule->GetWorldEpoch(),
			TaskOwnerId, TaskOwnerIncarnation, {}, ++NextTaskSerial);
	}
	auto& Attempt = State.SubscriptionAdmission;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Scratch;
	FVoxelGenerationRequiredSourceScope StateSource(State.Resources.Get(), State.ResourceLease.Get());
	FVoxelGenerationRequiredSourceScope TransferSource(State.Resources.Get(), Transfer.GetMetadataLease().Get());
	const uint64 Bytes = VoxelGenerationAllocation::Private::ArrayReserveBytes<FIntVector, FDefaultAllocator>(FVoxelOverlayRegionCodec::MaximumRegions) * 2;
	if (!GrantControlScratch(*State.Resources, Attempt, Bytes, Scratch)) return;
	FVoxelGenerationRequiredSourceScope ScratchSource(State.Resources.Get(), Scratch.Get());
	TArray<FIntVector> Scope;
	Scope.Reserve(FVoxelOverlayRegionCodec::MaximumRegions);
	const auto AddBounds = [&Scope, Bytes, &Attempt](FVoxelGenerationBounds Bounds)
	{
		Bounds.Min -= FIntVector(16);
		Bounds.Max += FIntVector(16);
		TArray<FIntVector> Keys;
		Keys.Reserve(FVoxelOverlayRegionCodec::MaximumRegions);
		if (VoxelGenerationAllocation::AllocatedBytes(Scope) + VoxelGenerationAllocation::AllocatedBytes(Keys) != Bytes)
		{
			Attempt.Admission = EVoxelTerrainResourceAdmission::InvalidRequest;
			return false;
		}
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
			if ((Source.GetCurrentWindow().Bounds.IsValid() && !AddBounds(Source.GetCurrentWindow().Bounds)) ||
				(Source.GetPendingWindow().IsSet() && !AddBounds(Source.GetPendingWindow()->Bounds)))
			{
				if (Attempt.Admission != EVoxelTerrainResourceAdmission::InvalidRequest)
					Attempt.Admission = EVoxelTerrainResourceAdmission::CapacityUnachievable;
				return;
			}
			for (const auto& Section : Source.DataSections)
			{
				const auto Key = FVoxelOverlayRegionCodec::RegionOf(Section);
				if (!Scope.Contains(Key))
				{
					if (Scope.Num() == FVoxelOverlayRegionCodec::MaximumRegions)
					{
						Attempt.Admission = EVoxelTerrainResourceAdmission::CapacityUnachievable;
						return;
					}
					Scope.Add(Key);
				}
			}
		}
	Scope.Sort(LessRegion);
	if (Scope.IsEmpty())
	{
		// 出生范围尚未发布时没有订阅请求，等待原兴趣范围推进。
		CancelAttempt();
		return;
	}
	if (Scope == State.Regions && bSamePublishedIdentity)
	{
		State.SeenCurrentRevision = Current->PublicationRevision;
		State.SeenDesiredRevision = Desired->PublicationRevision;
		State.SeenCurrentDemandRevision = Current->DemandRevision;
		State.SeenDesiredDemandRevision = Desired->DemandRevision;
		State.SeenCurrentResidencyRevision = Current->ResidencyRevision;
		State.SeenDesiredResidencyRevision = Desired->ResidencyRevision;
		CancelAttempt();
		return;
	}
	const uint64 ScopeBytes = VoxelGenerationAllocation::AllocatedBytes(Scope);
	if (ScopeBytes != Bytes / 2 || Scratch->TryResize(ScopeBytes) != EVoxelTerrainResourceAdmission::Granted)
	{
		Attempt.Admission = EVoxelTerrainResourceAdmission::InvalidRequest;
		return;
	}
	if (!Attempt.Identity.Ticket.IsValid()) Attempt.Identity.Ticket = FGuid::NewGuid();
	if (!State.bSubscriptionExpected)
	{
		FString ExpectError;
		// 使用原 subscription Consumer，且在实际 grant 之前冻结外部释放版本。
		Attempt.CapacityVersion = State.Resources->ReleaseVersionExcludingOwner(Attempt.Consumer);
		Attempt.Admission = Replica->TryExpectTicket(Attempt.Identity.Subscription, Attempt.Identity.Ticket, Scope, Attempt.Consumer, ExpectError);
		if (Attempt.Admission != EVoxelTerrainResourceAdmission::Granted) return;
		State.bSubscriptionExpected = true;
	}
	const auto AcceptedIdentity = Attempt.Identity;
	if (!SendOverlayControl(EVoxelMessage::SubscribeRegion, Scope))
	{
		if (Attempt.Admission == EVoxelTerrainResourceAdmission::CapacityUnachievable || Attempt.Admission == EVoxelTerrainResourceAdmission::InvalidRequest)
		{
			Replica->CancelTicket(AcceptedIdentity.Ticket);
			State.bSubscriptionExpected = false;
		}
		return;
	}
	// 只有真实发送接纳后才交接；旧 Ticket/Regions 始终服务容量等待。
	if (State.RetiringTicket.IsValid()) Replica->CancelTicket(State.RetiringTicket);
	State.RetiringTicket = State.Identity.Ticket;
	State.RetiringRegions = State.Regions;
	State.Identity = AcceptedIdentity;
	State.Regions = MoveTemp(Scope);
	State.bSubscriptionAttempt = State.bSubscriptionExpected = false;
	State.SeenCurrentRevision = Current->PublicationRevision;
	State.SeenDesiredRevision = Desired->PublicationRevision;
	State.SeenCurrentDemandRevision = Current->DemandRevision;
	State.SeenDesiredDemandRevision = Desired->DemandRevision;
	State.SeenCurrentResidencyRevision = Current->ResidencyRevision;
	State.SeenDesiredResidencyRevision = Desired->ResidencyRevision;
	State.LastControl = State.LastRetry = Now;
	State.LastProgress = 0;
	State.RetryCount = 0;
}

void UVoxelModuleNetworkComponent::DecodeOverlayMessage(const FVoxelWireMessage& Message, const bool bServer)
{
	auto* WorldModule = Module.Get();
	if (!OverlayNetwork || !WorldModule || Message.Payload.Num() > FVoxelOverlayRegionCodec::MaximumMessageBytes) return;
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	const uint64 NodeBytes = TVoxelNetworkQueue<TSharedPtr<FVoxelOverlayInboundWork, ESPMode::ThreadSafe>>::NodeBytes();
	const uint64 Bytes = VoxelBoundedAllocation::SharedBytes<FVoxelOverlayInboundWork>() +
		VoxelBoundedAllocation::ArrayBytes<uint8>(Message.Payload.Num()) +
		VoxelBoundedAllocation::ArrayBytes<FVoxelOverlayRegionHead>(FVoxelOverlayRegionCodec::MaximumRegions) +
		VoxelBoundedAllocation::ArrayBytes<FVoxelOverlayDirectoryEntry>(FVoxelOverlayRegionCodec::MaximumPageEntries) +
		VoxelBoundedAllocation::ArrayBytes<uint8>(FVoxelOverlayRegionCodec::MaximumPartBytes) + 128;
	if (OverlayNetwork->InboundBytes + Bytes > OverlayConnectionBytes ||
		!OverlayNetwork->Resources->Acquire(FVoxelGenerationConsumerKey::Section(WorldModule->GetWorldEpoch(), TaskOwnerId,
			TaskOwnerIncarnation, {}, ++NextTaskSerial), EVoxelTerrainResourcePool::Input, Bytes + NodeBytes, Lease, 1))
	{
		Fail(TEXT("Overlay 连接接收配额不足"));
		return;
	}
	auto NodeLease = Lease->Split(NodeBytes);
	if (!NodeLease) { Fail(TEXT("InvalidResource: inbound queue prepaid node")); return; }
	auto Work = MakeShared<FVoxelOverlayInboundWork, ESPMode::ThreadSafe>();
	Work->ResourceLease = MoveTemp(Lease);
	Work->Bytes.Reserve(Message.Payload.Num());
	Work->Bytes.Append(Message.Payload);
	Work->Kind = Message.Kind;
	Work->bServer = bServer;
	OverlayNetwork->InboundBytes += Bytes;
	if (!OverlayNetwork->Inbound.Enqueue(MoveTemp(Work), MoveTemp(NodeLease)))
		Fail(TEXT("InvalidResource: inbound queue node grant"));
}

void UVoxelModuleNetworkComponent::HandleOverlayMessage(const EVoxelMessage Kind, FVoxelOverlayRegionMessage&& Message,
	const bool bServer)
{
	auto* WorldModule = Module.Get();
	if (!OverlayNetwork || !WorldModule) return;
	auto& State = *OverlayNetwork;
	if (Message.WorldEpoch != State.Identity.WorldEpoch || Message.ManifestIdentity != State.Identity.Manifest ||
		Message.DirectoryEpoch != State.Identity.DirectoryEpoch)
	{
		if (Message.SubscriptionId == State.Identity.Subscription && Message.CoverageTicket == State.Identity.Ticket)
		{
			UE_LOG(LogTemp, Warning, TEXT("Voxel overlay current ticket identity rejected: controller=%s epoch=%llu/%llu directory=%s/%s manifestMatches=%d"),
				*GetPathNameSafe(GetOwner()), Message.WorldEpoch, State.Identity.WorldEpoch,
				*Message.DirectoryEpoch.ToString(), *State.Identity.DirectoryEpoch.ToString(), Message.ManifestIdentity == State.Identity.Manifest);
		}
		return;
	}
	if (!bServer)
	{
		FString Error;
		if (auto* Replica = WorldModule->GetOverlayReplica())
		{
			if (!Replica->Receive(Kind, MoveTemp(Message), Error))
			{
				UE_LOG(LogTemp, Warning, TEXT("Voxel overlay receive rejected: controller=%s kind=%d reason=%s"),
					*GetPathNameSafe(GetOwner()), int32(Kind), *Error);
				State.LastRetry = -10.0;
			}
		}
		return;
	}
	if (Kind == EVoxelMessage::SubscribeRegion || Kind == EVoxelMessage::ResyncRequest)
	{
		if (Message.Heads.IsEmpty() || Message.Heads.Num() > FVoxelOverlayRegionCodec::MaximumRegions || !GetAuthorizedObserver()) return;
		if (State.Identity.Subscription == Message.SubscriptionId && State.Identity.Ticket == Message.CoverageTicket && !State.Regions.IsEmpty())
		{
			// 原 Ticket 的 scope 冻结；同身份的重传不能悄悄替换失败尝试的输入范围。
			if (Message.Heads.Num() != State.Regions.Num()) return;
			for (const auto& Region : State.Regions)
				if (!Message.Heads.ContainsByPredicate([&Region](const auto& Head) { return Head.Region == Region; })) return;
			// 同 Ticket 的可靠重传复用在途切点；客户端重试间隔不能取消合法长任务。
			if (State.bNeedsSnapshot || State.bBuild || State.bEncode || State.SnapshotBuildHead || State.DeltaBuildHead ||
				State.Snapshot || State.EncodingDelta || State.SnapshotEncoder || State.DeltaEncoder || State.Outbound) return;
		}
		const FVector ObserverCell = GetAuthorizedObserver()->GetActorLocation() / WorldModule->BlockSize();
		TArray<FVoxelStreamingSource> Authorized;
		WorldModule->CollectStreamingSourcesForOwner(Controller(), Authorized);
		const auto HalfExtentFor = [WorldModule](const FVoxelStreamingSourceView& View)
		{
			const auto Window = WorldModule->GetViewSettings().Fine.WindowSettings(View);
			return FVector(double(Window.RadiusCells) + Window.ShiftStepCells + 128 + 16,
				double(Window.RadiusCells) + Window.ShiftStepCells + 128 + 16,
				double(Window.VerticalHalfExtentCells) + Window.VerticalShiftStepCells + 128 + 16);
		};
		const FVector HalfExtent = HalfExtentFor(WorldModule->GetViewSettings().DefaultSourceView);
		for (const auto& Head : Message.Heads)
		{
			const FBox Bounds(FVector(Head.Region * 128), FVector((Head.Region + FIntVector(1)) * 128));
			bool bAllowed = Bounds.Intersect(FBox(ObserverCell - HalfExtent, ObserverCell + HalfExtent));
			for (const auto& Source : Authorized)
			{
				const FVector SourceExtent = HalfExtentFor(Source.View);
				bAllowed |= Bounds.Intersect(FBox(FVector(Source.Center) - SourceExtent, FVector(Source.Center) + SourceExtent));
			}
			if (!bAllowed) { Fail(TEXT("Overlay 订阅超出已授权 Source 的当前或预热范围")); return; }
		}
		State.Identity.Subscription = Message.SubscriptionId;
		State.Identity.Ticket = Message.CoverageTicket;
		if (State.bBuild) WorldModule->CancelTerrainNetworkTask(State.BuildStamp);
		if (State.bEncode) WorldModule->CancelTerrainNetworkTask(State.EncodeStamp);
		State.BuildAdmission.Reset();
		State.EncodeAdmission.Reset();
		State.SnapshotBuildHead.Reset();
		State.DeltaBuildHead.Reset();
		State.DeltaBuildBasis.Reset();
		State.SnapshotFactoryAdmission = FVoxelOverlayControlAdmission();
		State.DeltaFactoryAdmission = FVoxelOverlayControlAdmission();
		State.DeltaFactoryTransaction.Invalidate();
		State.EncodeHead.Reset();
		++State.BuildSerial;
		State.bBuild = State.bEncode = false;
		State.AuthorityRoot.Reset();
		State.SnapshotEncoder.Reset();
		State.DeltaEncoder.Reset();
		State.Snapshot.Reset();
		State.EncodingDelta.Reset();
		ClearEncoderFactory(State);
		State.OutboundFactoryAdmission = FVoxelOverlayControlAdmission();
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
		// A repeated reliable gap request must not replace an admitted or capacity-waiting encoder attempt.
		if (State.SnapshotEncoder || State.DeltaEncoder || State.Snapshot || State.EncodingDelta) return;
		if (Retained && NextTaskSerial < MAX_uint64)
		{
			State.EncodingDelta = *Retained;
			SetEncoderFactory(State, FVoxelGenerationConsumerKey::Section(WorldModule->GetWorldEpoch(), TaskOwnerId,
				TaskOwnerIncarnation, {}, ++NextTaskSerial), (*Retained)->Root->Identity, EVoxelMessage::RegionDelta);
			CreatePendingOverlayEncoder(State, *WorldModule);
		}
		else State.bNeedsSnapshot = true;
	}
}

void UVoxelModuleNetworkComponent::BeginOverlaySnapshot()
{
	auto* WorldModule = Module.Get();
	if (!OverlayNetwork || !WorldModule || !WorldModule->IsAuthority() || OverlayNetwork->Regions.IsEmpty()) return;
	const auto StateOwner = OverlayNetwork;
	auto& State = *StateOwner;
	const uint64 AuthorityCut = WorldModule->GetRuntime()->GetAuthorityCommitCut();
	const auto Limits = State.Resources->Snapshot().Limits;
	if (State.bSnapshotTerminalFailure)
	{
		if (SameSubscriptionIdentity(State.SnapshotTerminalIdentity, State.Identity) &&
			State.SnapshotTerminalIdentity.Ticket == State.Identity.Ticket &&
			State.SnapshotTerminalIdentity.Cut == AuthorityCut && SameControlLimits(State.SnapshotTerminalLimits, Limits)) return;
		// 真正的 cut/ticket/config 输入变化才结束原失败记录并创建新尝试。
		State.bSnapshotTerminalFailure = false;
		State.SnapshotTerminalConsumer = FVoxelGenerationConsumerKey();
	}
	if (State.DeltaBuildHead || State.DeltaFactoryAdmission.Consumer.IsValid())
	{
		State.BuildAdmission.Reset();
		State.DeltaBuildHead.Reset();
		State.DeltaBuildBasis.Reset();
		State.DeltaFactoryAdmission = FVoxelOverlayControlAdmission();
		State.DeltaFactoryTransaction.Invalidate();
	}
	if (!State.SnapshotBuildHead)
	{
		auto Identity = State.Identity;
		Identity.Cut = AuthorityCut;
		auto& Attempt = State.SnapshotFactoryAdmission;
		if (!Attempt.Consumer.IsValid() || !SameControlIdentity(Attempt.Identity, Identity) || !SameControlLimits(Attempt.Limits, Limits))
		{
			Attempt = FVoxelOverlayControlAdmission();
			Attempt.Identity = Identity;
			Attempt.Limits = Limits;
			if (NextTaskSerial == MAX_uint64)
			{
				Attempt.Admission = EVoxelTerrainResourceAdmission::CapacityUnachievable;
				RecordSnapshotFactoryFailure(State);
				return;
			}
			Attempt.Consumer = FVoxelGenerationConsumerKey::Section(WorldModule->GetWorldEpoch(), TaskOwnerId,
				TaskOwnerIncarnation, State.Regions[0], ++NextTaskSerial);
		}
		const auto Consumer = Attempt.Consumer;
		TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> CaptureLease;
		FVoxelGenerationRequiredSourceScope StateSource(State.Resources.Get(), State.ResourceLease.Get());
		struct FRetainedRoot { const FVoxelTerrainResources& Resources; const FVoxelOverlayAuthorityDirectoryRoot* Root; } Retained{*State.Resources, State.AuthorityRoot.Get()};
		FVoxelGenerationRequiredSourceScope RootSource(State.Resources.Get(), &Retained,
			[](const void* Context, FVoxelGenerationRequiredSourceScope::FVisit Visit)
			{
				const auto& Source = *static_cast<const FRetainedRoot*>(Context);
				if (Source.Root) FVoxelOverlayAuthorityDeltaBuild::VisitRootSources(*Source.Root, Source.Resources, Visit);
			});
		const uint64 Metadata = VoxelGenerationAllocation::Private::ArrayReserveBytes<FVoxelOverlayAuthorityRegionCapture, FDefaultAllocator>(State.Regions.Num());
		if (!GrantFactoryInput(*State.Resources, Attempt, Metadata, CaptureLease))
		{
			RecordSnapshotFactoryFailure(State);
			return;
		}
		const uint64 BeforeCapture = Attempt.CapacityVersion;
		TArray<FVoxelOverlayAuthorityRegionCapture> Captures;
		Captures.Reserve(State.Regions.Num());
		if (VoxelGenerationAllocation::AllocatedBytes(Captures) != Metadata)
		{
			Attempt.Admission = EVoxelTerrainResourceAdmission::InvalidRequest;
			RecordSnapshotFactoryFailure(State);
			return;
		}
		struct FCaptureSources { const FVoxelTerrainResources& Resources; const TArray<FVoxelOverlayAuthorityRegionCapture>& Captures; const FVoxelTerrainResourceLease* Lease; } Sources{*State.Resources, Captures, CaptureLease.Get()};
		FVoxelGenerationRequiredSourceScope CapturedSources(State.Resources.Get(), &Sources,
			[](const void* Context, FVoxelGenerationRequiredSourceScope::FVisit Visit)
			{
				const auto& Sources = *static_cast<const FCaptureSources*>(Context);
				FVoxelGenerationRequiredSourceScope::VisitLease(Sources.Lease, Visit);
				for (const auto& Capture : Sources.Captures) FVoxelOverlayAuthoritySnapshotBuild::VisitCaptureSources(Capture.Capture, Sources.Resources, Visit);
			});
		for (const auto& Key : State.Regions)
		{
			FVoxelOverlayAuthorityRegionCapture Capture;
			Capture.Head.Region = Key;
			Capture.Head.Sequence = WorldModule->GetRuntime()->GetOverlayRegionSequence(Key);
			FString Error;
			if (!WorldModule->CaptureFineOverlays({Key * 128, (Key + FIntVector(1)) * 128}, Capture.Capture, Error, &Consumer))
			{
				Attempt.Admission = OverlayAdmissionFromError(Error);
				Attempt.CapacityVersion = BeforeCapture;
				RecordSnapshotFactoryFailure(State);
				return;
			}
			Captures.Add(MoveTemp(Capture));
		}
		FString Error;
		Attempt.Admission = FVoxelOverlayAuthoritySnapshotBuild::Create(Identity, MoveTemp(Captures),
			State.Resources.ToSharedRef(), Consumer, CaptureLease, State.SnapshotBuildHead, Error);
		if (Attempt.Admission != EVoxelTerrainResourceAdmission::Granted)
		{
			Attempt.CapacityVersion = BeforeCapture;
			RecordSnapshotFactoryFailure(State);
			return;
		}
		State.Identity.Cut = Identity.Cut;
		State.SnapshotBuildIdentity = Identity;
		State.SnapshotBuildLimits = Limits;
		++State.BuildSerial;
		State.SnapshotFactoryAdmission = FVoxelOverlayControlAdmission();
		State.BuildAdmission.Request.Emplace();
		auto& Task = State.BuildAdmission.Request.GetValue();
		Task.Kind = EVoxelTaskKind::NetworkRepresentation;
		Task.Stamp.OwnerId = TaskOwnerId;
		Task.Stamp.OwnerIncarnation = TaskOwnerIncarnation;
		Task.Stamp.WorldEpoch = WorldModule->GetWorldEpoch();
		Task.Stamp.Token = Consumer.RequestSerial;
		Task.Stamp.Section = State.Regions[0];
		State.BuildStamp = Task.Stamp;
		Task.ServiceBand = EVoxelTerrainServiceBand::P2;
		Task.WorkClass = EVoxelWorkClass::Visible;
		Task.SourceStableKey = TaskOwnerId;
		Task.FirstRunnableSeconds = FPlatformTime::Seconds();
		Task.bResumable = true;
		Task.ReservedBytes = 1;
	}
	const auto Build = State.SnapshotBuildHead;
	const auto Identity = State.SnapshotBuildIdentity;
	const uint64 BuildSerial = State.BuildSerial;

	auto GetBlockReason = [Build, Resources = State.Resources]
	{
		return Build->IsCapacityBlocked() && Resources->ReleaseVersionExcludingOwner(Build->GetConsumer()) <= Build->GetCapacityVersion() ?
			EVoxelTaskBlockReason::Memory : EVoxelTaskBlockReason::None;
	};
	auto Execute = [Build](const TAtomic<bool>& Cancel) { return Build->Advance(Cancel); };
	auto Apply = [Weak = TWeakObjectPtr<UVoxelModuleNetworkComponent>(this), Identity, BuildSerial,
		Limits = State.SnapshotBuildLimits, Consumer = Build->GetConsumer()](FVoxelTaskResult&& Result)
	{
		auto* Component = Weak.Get();
		if (!Component || !Component->OverlayNetwork || Component->TaskOwnerId != Result.Stamp.OwnerId ||
			Component->TaskOwnerIncarnation != Result.Stamp.OwnerIncarnation || !Component->Module.IsValid() ||
			Component->Module->GetWorldEpoch() != Result.Stamp.WorldEpoch || Component->OverlayNetwork->BuildStamp != Result.Stamp) return;
		auto& Current = *Component->OverlayNetwork;
		auto* CurrentModule = Component->Module.Get();
		if (!CurrentModule || CurrentModule->GetWorldEpoch() != Result.Stamp.WorldEpoch ||
			Current.BuildSerial != BuildSerial || Current.Identity.Ticket != Identity.Ticket ||
			Current.Identity.Subscription != Identity.Subscription) return;
		Current.bBuild = false;
		if (!Result.bSuccess || Result.bCanceled || !Result.CustomPayload)
		{
			if (!Result.bCanceled && (Result.Error.StartsWith(TEXT("CapacityUnachievable:")) ||
				Result.Error.StartsWith(TEXT("Oversized:")) || Result.Error.StartsWith(TEXT("InvalidResource:")) ||
				Result.Error.StartsWith(TEXT("InvalidRequest:")) || Result.Error.StartsWith(TEXT("EstimateMismatch:"))))
			{
				Current.SnapshotTerminalIdentity = Identity;
				Current.SnapshotTerminalLimits = Limits;
				Current.SnapshotTerminalConsumer = Consumer;
				Current.bSnapshotTerminalFailure = true;
			}
			Current.bNeedsSnapshot = true;
			return;
		}
		Current.bSnapshotTerminalFailure = false;
		Current.SnapshotTerminalConsumer = FVoxelGenerationConsumerKey();
		Current.Snapshot = StaticCastSharedPtr<const FVoxelOverlayAuthorityPrepared>(Result.CustomPayload);
		SetEncoderFactory(Current, Consumer, Current.Snapshot->Root->Identity, EVoxelMessage::SnapshotBegin);
		CreatePendingOverlayEncoder(Current, *CurrentModule);
	};
	FString BindingError;
	const auto BindingResources = WorldModule->GetTerrainResources();
	if (!BindingResources) return;
	if (!State.BuildAdmission.Bind(BindingResources.ToSharedRef(), WorldModule->GetTerrainTaskControlConsumer(),
		MoveTemp(GetBlockReason), MoveTemp(Execute), MoveTemp(Apply), BindingError))
	{
		if (!BindingError.StartsWith(TEXT("CapacityBlocked"))) Fail(BindingError);
		return;
	}
	if (State.BuildAdmission.Enqueue([WorldModule](FVoxelTaskRequest&& Request)
		{ return WorldModule->EnqueueNetworkRepresentationTask(MoveTemp(Request)); }, BindingResources.ToSharedRef(),
		WorldModule->GetTerrainTaskControlConsumer(), BindingError))
	{
		if (OverlayNetwork != StateOwner || State.SnapshotBuildHead != Build) return;
		State.BuildAdmission.Reset();
		State.SnapshotBuildHead.Reset();
		State.LastRetry = FPlatformTime::Seconds();
		State.bBuild = true;
		State.bNeedsSnapshot = false;
		UE_LOG(LogTemp, Display, TEXT("Voxel overlay snapshot admitted: controller=%s serial=%llu regions=%d transferQueuedBytes=%llu"),
			*GetPathNameSafe(GetOwner()), State.BuildSerial, State.Regions.Num(), Transfer.QueuedBytes());
		for (int32 Index = State.Edits.Num() - 1; Index >= 0; --Index)
			if (State.Edits[Index].Cut <= Identity.Cut) State.Edits.RemoveAt(Index, 1, EAllowShrinking::No);
		State.Replay.Reset();
		State.ReplayBytes = 0;
		State.Acknowledged.Reset();
		State.SnapshotEncoder.Reset();
		State.DeltaEncoder.Reset();
		State.Snapshot.Reset();
		State.EncodingDelta.Reset();
		ClearEncoderFactory(State);
		State.OutboundFactoryAdmission = FVoxelOverlayControlAdmission();
		State.Outbound.Reset();
		State.bEncode = false;
	}
	else if (!BindingError.IsEmpty()) Fail(BindingError);
}

void UVoxelModuleNetworkComponent::CaptureOverlayCommit(const FVoxelEditBatch& Batch)
{
	auto* WorldModule = Module.Get();
	if (!OverlayNetwork || !WorldModule || !WorldModule->IsAuthority() || !OverlayNetwork->Identity.Ticket.IsValid()) return;
	auto& State = *OverlayNetwork;
	if ((State.bNeedsSnapshot && !State.SnapshotBuildHead) || Batch.Sections.IsEmpty()) return;
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
	const auto StateOwner = OverlayNetwork;
	auto& State = *StateOwner;
	const bool bServer = GetOwner() && GetOwner()->HasAuthority();
	if (!State.bDecode)
	{
		if (State.InboundHead || State.Inbound.Dequeue(State.InboundHead))
		{
			const auto Work = State.InboundHead;
			if (!State.DecodeAdmission.Request.IsSet())
			{
				State.DecodeAdmission.Request.Emplace();
				auto& Task = State.DecodeAdmission.Request.GetValue();
				Task.Kind = EVoxelTaskKind::NetworkRepresentation;
				Task.Stamp.WorldEpoch = WorldModule->GetWorldEpoch();
				Task.Stamp.OwnerId = TaskOwnerId;
				Task.Stamp.OwnerIncarnation = TaskOwnerIncarnation;
				Task.Stamp.Token = ++NextTaskSerial;
				Task.WorkClass = EVoxelWorkClass::Visible;
				Task.ServiceBand = EVoxelTerrainServiceBand::P2;
				Task.SourceStableKey = TaskOwnerId;
				Task.FirstRunnableSeconds = Now;
				Task.ReservedBytes = 1;
			}

			auto Execute = [Work](const TAtomic<bool>& Cancel)
			{
				FVoxelTaskResult Result;
				if (Cancel.Load()) { Result.bCanceled = true; return Result; }
				Work->Message.ResourceLease = Work->ResourceLease;
				Result.bSuccess = FVoxelOverlayRegionCodec::Decode(Work->Kind, Work->Bytes, Work->Message, Result.Error);
				if (Result.bSuccess) Result.CustomPayload = Work;
				return Result;
			};
			auto Apply = [Weak = TWeakObjectPtr<UVoxelModuleNetworkComponent>(this), Work](FVoxelTaskResult&& Result)
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
			FString BindingError;
			const auto BindingResources = WorldModule->GetTerrainResources();
			if (BindingResources && State.DecodeAdmission.Bind(BindingResources.ToSharedRef(), WorldModule->GetTerrainTaskControlConsumer(),
				[] { return EVoxelTaskBlockReason::None; }, MoveTemp(Execute), MoveTemp(Apply), BindingError))
			{
				if (State.DecodeAdmission.Enqueue([WorldModule](FVoxelTaskRequest&& Request)
					{ return WorldModule->EnqueueNetworkRepresentationTask(MoveTemp(Request)); }, BindingResources.ToSharedRef(),
					WorldModule->GetTerrainTaskControlConsumer(), BindingError))
				{
					if (OverlayNetwork != StateOwner || State.InboundHead != Work) return;
					State.bDecode = true;
					State.DecodeAdmission.Reset();
					State.InboundHead.Reset();
				}
				else if (!BindingError.IsEmpty())
				{
					Fail(BindingError);
					return;
				}
			}
			else if (!BindingError.IsEmpty() && !BindingError.StartsWith(TEXT("CapacityBlocked")))
			{
				Fail(BindingError);
				return;
			}
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
		bool bIngressRetryReady = true;
		if (State.bIngressWaitingCapacity)
		{
			uint64 CurrentVersion = 0;
			bIngressRetryReady = State.Resources->TryGetReleaseVersionExcludingOwner(State.IngressCapacityConsumer, CurrentVersion) &&
				CurrentVersion > State.IngressCapacityVersion;
			if (bIngressRetryReady) State.bIngressWaitingCapacity = false;
		}
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
		if (Progress != State.LastProgress)
		{
			State.LastProgress = Progress;
			if (!State.bIngressResyncPending)
			{
				State.RetryCount = 0;
				State.LastRetry = Now;
			}
		}
		bComplete &= !State.bIngressResyncPending;
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
		else if (bIngressRetryReady && Now - State.LastRetry >= FMath::Min(2.0, 0.25 * FMath::Pow(2.0, State.RetryCount)))
		{
			const auto Kind = State.bIngressResyncPending || State.RetryCount >= 3 || !bGap ? EVoxelMessage::ResyncRequest : EVoxelMessage::GapRequest;
			if (SendOverlayControl(Kind, State.Regions))
			{
				State.LastRetry = Now;
				State.RetryCount = FMath::Min(State.RetryCount + 1, 4);
				State.bIngressResyncPending = false;
			}
		}
		return;
	}
	if (!State.bBuild && !CreatePendingOverlayEncoder(State, *WorldModule)) return;
	if (State.bNeedsSnapshot && !State.bBuild)
	{
		uint64 CurrentVersion = 0;
		if (!State.bIngressWaitingCapacity ||
			(State.Resources->TryGetReleaseVersionExcludingOwner(State.IngressCapacityConsumer, CurrentVersion) &&
				CurrentVersion > State.IngressCapacityVersion))
		{
			State.bIngressWaitingCapacity = false;
			BeginOverlaySnapshot();
			if (State.bBuild) State.bIngressResyncPending = false;
			return;
		}
	}
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
		if (!State.EncodeHead)
		{
			const auto EncoderConsumer = State.SnapshotEncoder ? State.SnapshotEncoder->GetConsumer() : State.DeltaEncoder->GetConsumer();
			const auto EncoderIdentity = State.Snapshot ? State.Snapshot->Root->Identity : State.EncodingDelta->Root->Identity;
			auto& Attempt = State.OutboundFactoryAdmission;
			if (!Attempt.Consumer.IsValid())
			{
				Attempt.Consumer = EncoderConsumer;
				Attempt.Identity = EncoderIdentity;
				Attempt.Limits = State.Resources->Snapshot().Limits;
				Attempt.Kind = State.SnapshotEncoder ? EVoxelMessage::SnapshotBegin : EVoxelMessage::RegionDelta;
			}
			if ((Attempt.Admission == EVoxelTerrainResourceAdmission::CapacityUnachievable || Attempt.Admission == EVoxelTerrainResourceAdmission::InvalidRequest) &&
				(WorldModule->GetRuntime()->GetAuthorityCommitCut() != Attempt.Identity.Cut || !SameControlLimits(Attempt.Limits, State.Resources->Snapshot().Limits)))
			{
				State.SnapshotEncoder.Reset(); State.DeltaEncoder.Reset();
				State.Snapshot.Reset(); State.EncodingDelta.Reset();
				ClearEncoderFactory(State);
				Attempt = FVoxelOverlayControlAdmission();
				State.bNeedsSnapshot = true;
				return;
			}
			if (!ResumeControl(*State.Resources, Attempt)) return;
			if (NextTaskSerial == MAX_uint64) { Attempt.Admission = EVoxelTerrainResourceAdmission::CapacityUnachievable; return; }
			FVoxelGenerationRequiredSourceScope StateSource(State.Resources.Get(), State.ResourceLease.Get());
			struct FInput { const FVoxelOverlayNetworkState& State; } Input{State};
			FVoxelGenerationRequiredSourceScope EncoderSource(State.Resources.Get(), &Input,
				[](const void* Context, FVoxelGenerationRequiredSourceScope::FVisit Visit)
				{
					const auto& State = static_cast<const FInput*>(Context)->State;
					if (State.AuthorityRoot) FVoxelOverlayAuthorityDeltaBuild::VisitRootSources(*State.AuthorityRoot, *State.Resources, Visit);
					if (State.SnapshotEncoder) State.SnapshotEncoder->VisitResourceSources(*State.Resources, Visit);
					if (State.DeltaEncoder) State.DeltaEncoder->VisitResourceSources(*State.Resources, Visit);
				});
			TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
			if (!GrantFactoryInput(*State.Resources, Attempt, VoxelBoundedAllocation::WorkerSharedBytes<FVoxelOverlayOutboundWork>(), Lease)) return;
			State.EncodeConsumer = EncoderConsumer;
			State.EncodeHead = VoxelBoundedAllocation::MakeWorkerShared<FVoxelOverlayOutboundWork>(Lease);
			State.EncodeHead->ResourceLease = MoveTemp(Lease);
			State.EncodeHead->ResourceLease->MarkPublished();
			State.OutboundFactoryAdmission = FVoxelOverlayControlAdmission();
			State.EncodeAdmission.Request.Emplace();
			auto& Task = State.EncodeAdmission.Request.GetValue();
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
			Task.ReservedBytes = 1;
		}
		const auto Work = State.EncodeHead;
		const auto SnapshotEncoder = State.SnapshotEncoder;
		const auto DeltaEncoder = State.DeltaEncoder;
		const auto Consumer = State.EncodeConsumer;

		auto GetBlockReason = [Work, Resources = State.Resources, Consumer]
		{
			return Work->bCapacityBlocked.Load() && Resources->ReleaseVersionExcludingOwner(Consumer) <= Work->CapacityVersion.Load() ?
				EVoxelTaskBlockReason::Memory : EVoxelTaskBlockReason::None;
		};
		auto Execute = [Work, SnapshotEncoder, DeltaEncoder, Resources = State.Resources, Consumer, EnvelopeSession = Session](const TAtomic<bool>& Cancel)
		{
			FVoxelTaskResult Result;
			if (Cancel.Load()) { Result.bCanceled = true; return Result; }
			const uint64 BeforeBodyAdmission = Resources->ReleaseVersionExcludingOwner(Consumer);
			struct FInput
			{
				const FVoxelTerrainResources& Resources;
				const FVoxelOverlayOutboundWork& Work;
				const FVoxelOverlaySnapshotEncoder* Snapshot;
				const FVoxelOverlayDeltaEncoder* Delta;
			} Input{*Resources, *Work, SnapshotEncoder.Get(), DeltaEncoder.Get()};
			FVoxelGenerationRequiredSourceScope Sources(Resources.Get(), &Input,
				[](const void* Context, FVoxelGenerationRequiredSourceScope::FVisit Visit)
				{
					const auto& Input = *static_cast<const FInput*>(Context);
					for (const auto* Lease : {Input.Work.ResourceLease.Get(), Input.Work.WireLease.Get(), Input.Work.Message.ResourceLease.Get()})
						if (Lease && Lease->IsOwnedBy(Input.Resources)) FVoxelGenerationRequiredSourceScope::VisitLease(Lease, Visit);
					if (Input.Snapshot) Input.Snapshot->VisitResourceSources(Input.Resources, Visit);
					if (Input.Delta) Input.Delta->VisitResourceSources(Input.Resources, Visit);
				});
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
					const uint64 Capacity = VoxelGenerationAllocation::Private::ArrayReserveBytes<uint8, FDefaultAllocator>(Work->Message.Bytes.Num() + 29);
					FVoxelGenerationRequiredCapacity Required;
					EVoxelTerrainResourceAdmission Admission = EVoxelTerrainResourceAdmission::InvalidRequest;
					if (FVoxelGenerationRequiredSourceScope::Collect(*Resources, nullptr, Required, Result.Error))
					{
						const uint64 StagedLimit = Resources->Snapshot().Limits.StagedBytes;
						const uint64 Retained = Required.Bytes[int32(EVoxelTerrainResourcePool::Staged)];
						Admission = Retained > StagedLimit || Capacity > StagedLimit - Retained ? EVoxelTerrainResourceAdmission::CapacityUnachievable :
							Resources->TryAcquire(Consumer, EVoxelTerrainResourcePool::Staged, Capacity, Work->WireLease, 0, Required.Bytes[0], Required.LeaseCount);
					}
					if (Admission != EVoxelTerrainResourceAdmission::Granted)
					{
						Result.bSuccess = false;
						Result.Error = Admission == EVoxelTerrainResourceAdmission::CapacityBlocked ? TEXT("CapacityBlocked: 发送 Envelope native body") :
							Admission == EVoxelTerrainResourceAdmission::CapacityUnachievable ? TEXT("CapacityUnachievable: 发送 Envelope native body") :
							TEXT("InvalidResource: 发送 Envelope native body");
					}
					else if (!FVoxelNetworkCodec::Encode(Work->Message.Kind, EnvelopeSession, Work->Message.Bytes, Work->WireBytes) ||
						VoxelGenerationAllocation::AllocatedBytes(Work->WireBytes) > Work->WireLease->Bytes())
					{
						Result.bSuccess = false;
						Result.Error = TEXT("InvalidResource: 发送 Envelope 编码或授予校验失败");
					}
					else
					{
						Work->WireLease->Resize(VoxelGenerationAllocation::AllocatedBytes(Work->WireBytes));
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
				Work->CapacityVersion.Store(BeforeBodyAdmission);
				Work->bCapacityBlocked.Store(true);
				Result.bSuccess = Result.bContinue = true;
				Result.Error.Reset();
			}
			else if (Result.bSuccess) { Work->bCapacityBlocked.Store(false); Result.CustomPayload = Work; }
			return Result;
		};
		auto Apply = [Weak = TWeakObjectPtr<UVoxelModuleNetworkComponent>(this), Work, SnapshotEncoder, DeltaEncoder, Consumer](FVoxelTaskResult&& Result)
		{
			auto* Component = Weak.Get();
			if (!Component || !Component->OverlayNetwork || !Component->Module.IsValid() || Component->TaskOwnerId != Result.Stamp.OwnerId ||
				Component->TaskOwnerIncarnation != Result.Stamp.OwnerIncarnation || Component->Module->GetWorldEpoch() != Result.Stamp.WorldEpoch) return;
			auto& Current = *Component->OverlayNetwork;
			if (Current.SnapshotEncoder != SnapshotEncoder || Current.DeltaEncoder != DeltaEncoder) return;
			if (Current.EncodeStamp != Result.Stamp) return;
			Current.bEncode = false;
			if (Result.bSuccess && !Result.bCanceled)
			{
				if (Work->WireLease) Work->WireLease->MarkPublished();
				Current.Outbound = Work;
			}
			else
			{
				Current.OutboundFactoryAdmission.Consumer = Consumer;
				Current.OutboundFactoryAdmission.Identity = Current.Snapshot ? Current.Snapshot->Root->Identity : Current.EncodingDelta->Root->Identity;
				Current.OutboundFactoryAdmission.Limits = Current.Resources->Snapshot().Limits;
				Current.OutboundFactoryAdmission.Kind = SnapshotEncoder ? EVoxelMessage::SnapshotBegin : EVoxelMessage::RegionDelta;
				Current.OutboundFactoryAdmission.Admission = Result.bCanceled ? EVoxelTerrainResourceAdmission::InvalidRequest : OverlayAdmissionFromError(Result.Error);
			}
		};
		FString BindingError;
		const auto BindingResources = WorldModule->GetTerrainResources();
		if (!BindingResources) return;
		struct FBindingInput
		{
			const FVoxelTerrainResources& Resources;
			const FVoxelOverlayOutboundWork& Work;
			const FVoxelOverlaySnapshotEncoder* Snapshot;
			const FVoxelOverlayDeltaEncoder* Delta;
		} BindingInput{*BindingResources, *Work, SnapshotEncoder.Get(), DeltaEncoder.Get()};
		FVoxelGenerationRequiredSourceScope BindingSource(BindingResources.Get(), &BindingInput,
			[](const void* Context, FVoxelGenerationRequiredSourceScope::FVisit Visit)
			{
				const auto& Input = *static_cast<const FBindingInput*>(Context);
				for (const auto* Lease : {Input.Work.ResourceLease.Get(), Input.Work.WireLease.Get(), Input.Work.Message.ResourceLease.Get()})
					if (Lease && Lease->IsOwnedBy(Input.Resources)) FVoxelGenerationRequiredSourceScope::VisitLease(Lease, Visit);
				if (Input.Snapshot) Input.Snapshot->VisitResourceSources(Input.Resources, Visit);
				if (Input.Delta) Input.Delta->VisitResourceSources(Input.Resources, Visit);
			});
		FVoxelGenerationRequiredCapacity BindingRequired;
		if (!FVoxelGenerationRequiredSourceScope::Collect(*BindingResources, nullptr, BindingRequired, BindingError)) { Fail(BindingError); return; }
		State.EncodeAdmission.Request.GetValue().SetRequiredBindingInput(BindingRequired.Bytes[int32(EVoxelTerrainResourcePool::Input)], BindingRequired.LeaseCount);
		if (!State.EncodeAdmission.Bind(BindingResources.ToSharedRef(), WorldModule->GetTerrainTaskControlConsumer(),
			MoveTemp(GetBlockReason), MoveTemp(Execute), MoveTemp(Apply), BindingError))
		{
			if (!BindingError.StartsWith(TEXT("CapacityBlocked"))) Fail(BindingError);
			return;
		}
		if (State.EncodeAdmission.Enqueue([WorldModule](FVoxelTaskRequest&& Request)
			{ return WorldModule->EnqueueNetworkRepresentationTask(MoveTemp(Request)); }, BindingResources.ToSharedRef(),
			WorldModule->GetTerrainTaskControlConsumer(), BindingError))
		{
			if (OverlayNetwork != StateOwner || State.EncodeHead != Work) return;
			State.bEncode = true;
			State.EncodeAdmission.Reset();
			State.EncodeHead.Reset();
		}
		else if (!BindingError.IsEmpty()) Fail(BindingError);
		return;
	}
	if (State.bBuild || State.SnapshotEncoder || State.DeltaEncoder ||
		(!State.DeltaBuildHead && State.Edits.IsEmpty()) || !State.AuthorityRoot) return;
	if (State.ReplayBytes >= OverlayConnectionBytes || State.Replay.Num() >= 256) { State.bNeedsSnapshot = true; return; }
	if (!State.DeltaBuildHead)
	{
		auto& Edit = State.Edits[0];
		if (Edit.Cut <= State.AuthorityRoot->Identity.Cut)
		{
			State.Edits.RemoveAt(0, 1, EAllowShrinking::No);
			State.DeltaFactoryAdmission = FVoxelOverlayControlAdmission();
			State.DeltaFactoryTransaction.Invalidate();
			State.DeltaBuildBasis.Reset();
			return;
		}
		auto Identity = State.AuthorityRoot->Identity;
		Identity.Cut = Edit.Cut;
		const auto Limits = State.Resources->Snapshot().Limits;
		auto& Attempt = State.DeltaFactoryAdmission;
		if (!Attempt.Consumer.IsValid() || State.DeltaFactoryTransaction != Edit.Transaction ||
			State.DeltaBuildBasis != State.AuthorityRoot || !SameControlIdentity(Attempt.Identity, Identity) || !SameControlLimits(Attempt.Limits, Limits))
		{
			Attempt = FVoxelOverlayControlAdmission();
			Attempt.Identity = Identity;
			Attempt.Limits = Limits;
			State.DeltaFactoryTransaction = Edit.Transaction;
			State.DeltaBuildBasis = State.AuthorityRoot;
			if (NextTaskSerial == MAX_uint64) { Attempt.Admission = EVoxelTerrainResourceAdmission::CapacityUnachievable; State.bNeedsSnapshot = true; return; }
			Attempt.Consumer = FVoxelGenerationConsumerKey::Section(WorldModule->GetWorldEpoch(), TaskOwnerId,
				TaskOwnerIncarnation, {}, ++NextTaskSerial);
		}
		if (!ResumeControl(*State.Resources, Attempt)) return;
		const auto Consumer = Attempt.Consumer;
		const uint64 BeforeFactory = Attempt.CapacityVersion;
		FVoxelGenerationRequiredSourceScope StateSource(State.Resources.Get(), State.ResourceLease.Get());
		FString Error;
		Attempt.Admission = FVoxelOverlayAuthorityDeltaBuild::Create(State.DeltaBuildBasis.ToSharedRef(), MoveTemp(Edit),
			State.Resources.ToSharedRef(), Consumer, State.DeltaBuildHead, Error);
		if (Attempt.Admission != EVoxelTerrainResourceAdmission::Granted)
		{
			Attempt.CapacityVersion = BeforeFactory;
			if (Attempt.Admission == EVoxelTerrainResourceAdmission::CapacityUnachievable || Attempt.Admission == EVoxelTerrainResourceAdmission::InvalidRequest)
				State.bNeedsSnapshot = true;
			return;
		}
		State.Edits.RemoveAt(0, 1, EAllowShrinking::No);
		State.DeltaFactoryAdmission = FVoxelOverlayControlAdmission();
		State.DeltaFactoryTransaction.Invalidate();
		State.BuildAdmission.Request.Emplace();
		auto& Task = State.BuildAdmission.Request.GetValue();
		Task.Kind = EVoxelTaskKind::NetworkRepresentation;
		Task.Stamp.WorldEpoch = WorldModule->GetWorldEpoch();
		Task.Stamp.OwnerId = TaskOwnerId;
		Task.Stamp.OwnerIncarnation = TaskOwnerIncarnation;
		Task.Stamp.Token = Consumer.RequestSerial;
		Task.WorkClass = EVoxelWorkClass::Visible;
		State.BuildStamp = Task.Stamp;
		Task.ServiceBand = EVoxelTerrainServiceBand::P2;
		Task.SourceStableKey = TaskOwnerId;
		Task.FirstRunnableSeconds = Now;
		Task.bResumable = true;
		Task.ReservedBytes = 1;
	}
	const auto Build = State.DeltaBuildHead;
	const auto Basis = State.DeltaBuildBasis;

	auto GetBlockReason = [Build, Resources = State.Resources]
	{
		return Build->IsCapacityBlocked() && Resources->ReleaseVersionExcludingOwner(Build->GetConsumer()) <= Build->GetCapacityVersion() ?
			EVoxelTaskBlockReason::Memory : EVoxelTaskBlockReason::None;
	};
	auto Execute = [Build](const TAtomic<bool>& Cancel) { return Build->Advance(Cancel); };
	auto Apply = [Weak = TWeakObjectPtr<UVoxelModuleNetworkComponent>(this), Basis](FVoxelTaskResult&& Result)
	{
		auto* Component = Weak.Get();
		if (!Component || !Component->OverlayNetwork || Component->TaskOwnerId != Result.Stamp.OwnerId ||
			Component->TaskOwnerIncarnation != Result.Stamp.OwnerIncarnation || !Component->Module.IsValid() ||
			Component->Module->GetWorldEpoch() != Result.Stamp.WorldEpoch || Component->OverlayNetwork->BuildStamp != Result.Stamp) return;
		auto& Current = *Component->OverlayNetwork;
		if (Current.AuthorityRoot != Basis) return;
		Current.bBuild = false;
		if (!Result.bSuccess || Result.bCanceled || !Result.CustomPayload) { Current.bNeedsSnapshot = true; return; }
		const auto Delta = StaticCastSharedPtr<const FVoxelOverlayAuthorityDelta>(Result.CustomPayload);
		const uint64 Bytes = Delta->Payload->Bytes.GetAllocatedSize() + Delta->Heads.GetAllocatedSize();
		if (Current.ReplayBytes + Bytes > OverlayConnectionBytes) { Current.bNeedsSnapshot = true; return; }
		Current.EncodingDelta = Delta;
		SetEncoderFactory(Current, FVoxelGenerationConsumerKey::Section(Result.Stamp.WorldEpoch, Result.Stamp.OwnerId,
			Result.Stamp.OwnerIncarnation, {}, Result.Stamp.Token), Delta->Root->Identity, EVoxelMessage::RegionDelta);
		Current.bEncoderDeltaCommitPending = true;
		CreatePendingOverlayEncoder(Current, *Component->Module.Get());
	};
	FString BindingError;
	const auto BindingResources = WorldModule->GetTerrainResources();
	if (!BindingResources) return;
	if (!State.BuildAdmission.Bind(BindingResources.ToSharedRef(), WorldModule->GetTerrainTaskControlConsumer(),
		MoveTemp(GetBlockReason), MoveTemp(Execute), MoveTemp(Apply), BindingError))
	{
		if (!BindingError.StartsWith(TEXT("CapacityBlocked"))) Fail(BindingError);
		return;
	}
	if (State.BuildAdmission.Enqueue([WorldModule](FVoxelTaskRequest&& Request)
		{ return WorldModule->EnqueueNetworkRepresentationTask(MoveTemp(Request)); }, BindingResources.ToSharedRef(),
		WorldModule->GetTerrainTaskControlConsumer(), BindingError))
	{
		if (OverlayNetwork != StateOwner || State.DeltaBuildHead != Build) return;
		State.bBuild = true;
		State.BuildAdmission.Reset();
		State.DeltaBuildHead.Reset();
		State.DeltaBuildBasis.Reset();
	}
	else if (!BindingError.IsEmpty()) Fail(BindingError);
}

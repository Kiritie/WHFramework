#include "Voxel/Network/VoxelModuleNetworkComponent.h"
#include "Voxel/Network/VoxelNetworkCodec.h"
#include "Voxel/Generation/VoxelManifestCodec.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/Runtime/VoxelStreaming.h"
#include "Ability/Inventory/AbilityInventoryAgentInterface.h"
#include "Ability/Inventory/AbilityInventoryBase.h"
#include "Ability/Inventory/Slot/AbilityInventorySlotBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
UVoxelModuleNetworkComponent::UVoxelModuleNetworkComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = true;
}
APlayerController* UVoxelModuleNetworkComponent::Controller() const
{
	return Cast<APlayerController>(GetOwner());
}
void UVoxelModuleNetworkComponent::BeginPlay()
{
	Super::BeginPlay();
	Nonce = Controller() && !Controller()->HasAuthority() ? FGuid::NewGuid() : FGuid();
	BindModule();
}
void UVoxelModuleNetworkComponent::BindModule()
{
	UVoxelModule* M = &UVoxelModule::Get();
	if (Module.Get() == M)
		return;
	if (auto* Old = Module.Get())
	{
		Old->OnBlocksCommitted.Remove(CommitHandle);
		Old->OnRemoteBatchCompleted.Remove(RemoteHandle);
		if (SourceId.IsValid())
			Old->UnregisterSource(SourceId);
	}
	Module = M;
	SourceId.Invalidate();
	if (M)
	{
		CommitHandle = M->OnBlocksCommitted.AddUObject(this, &UVoxelModuleNetworkComponent::OnCommit);
		RemoteHandle = M->OnRemoteBatchCompleted.AddUObject(this, &UVoxelModuleNetworkComponent::OnRemoteCompleted);
	}
}
void UVoxelModuleNetworkComponent::EndPlay(const EEndPlayReason::Type R)
{
	ResetPhase2();
	if (auto* M = Module.Get())
	{
		M->OnBlocksCommitted.Remove(CommitHandle);
		M->OnRemoteBatchCompleted.Remove(RemoteHandle);
		M->UnregisterSource(SourceId);
	}
	Transfer.Reset();
	Module.Reset();
	Super::EndPlay(R);
}
void UVoxelModuleNetworkComponent::ResetProtocol()
{
	ResetPhase2();
	bReady = bWelcomed = false;
	Session.Invalidate();
	Acknowledged.Reset();
	LastSent.Reset();
	PendingBatches.Reset();
	DirtyTransactions.Reset();
	Interest.Reset();
	Transfer.Reset();
	if (auto* M = Module.Get())
		M->UnregisterSource(SourceId);
	SourceId.Invalidate();
	LastHello = LastInterest = -10;
	LastRequestId = 0;
}
void UVoxelModuleNetworkComponent::Fail(const FString& E)
{
	ResetPhase2();
	Transfer.Reset();
	if (auto* PC = Controller())
		if (PC->HasAuthority())
		{
			FVoxelByteWriter W(1024);
			W.String(E, 768);
			TArray<uint8> B;
			W.Finish(B);
			Send(EVoxelMessage::Closed, B);
		}
	bRejected = true;
	bReady = false;
	if (auto* M = Module.Get())
	{
		M->UnregisterSource(SourceId);
		if (!M->IsAuthority())
		{
			FString Error;
			M->StopWorld(true, Error);
		}
	}
	OnProtocolFailure.Broadcast(E);
	UE_LOG(LogTemp, Error, TEXT("Voxel protocol rejected: %s"), *E);
}
bool UVoxelModuleNetworkComponent::Send(EVoxelMessage K, const TArray<uint8>& P)
{
	TArray<uint8> B;
	if (!FVoxelNetworkCodec::Encode(K, Session, P, B))
		return false;
	return Transfer.Enqueue(MoveTemp(B));
}
void UVoxelModuleNetworkComponent::TickComponent(float D, ELevelTick T, FActorComponentTickFunction* F)
{
	Super::TickComponent(D, T, F);
	auto* PC = Controller();
	auto* M = Module.Get();
	if (!PC || !M)
		return;
	double Now = FPlatformTime::Seconds();
	bool Server = PC->HasAuthority();
	if (Server && PC->IsLocalController())
	{
		bReady = M->IsReady();
		return;
	}
	Transfer.Tick(Now,
	              [this, Server](const TArray<uint8>& B)
	              {
		              FVoxelRPCPacket P;
		              P.Bytes = B;
		              if (Server)
			              ClientReceive(P);
		              else
			              ServerReceive(P);
	              });
	if (bRejected)
		return;
	PumpPhase2();
	if (Server)
	{
		if (M->IsReady() && bWelcomed && Session != M->GetSessionId())
		{
			Fail(TEXT("Server world changed; reconnect to the new session"));
			return;
		}
		if (Nonce.IsValid() && !bWelcomed && M->IsReady())
		{
			Session = M->GetSessionId();
			FVoxelByteWriter W(FVoxelManifestCodec::MaxBytes + 64);
			W.Guid(Nonce);
			TArray<uint8> B;
			FVoxelManifestCodec::Encode(M->GetManifest(), B);
			W.Blob(B, FVoxelManifestCodec::MaxBytes);
			TArray<uint8> P;
			W.Finish(P);
			if (Send(EVoxelMessage::Welcome, P))
				bWelcomed = true;
		}
		if (!bReady)
			return;
		TArray<FGuid> Expired;
		for (const auto& P : PendingBatches)
			if (Now - P.Value.CreatedAt > 90)
				Expired.Add(P.Key);
		for (const auto& ID : Expired)
		{
			if (const auto* P = PendingBatches.Find(ID))
			{
				TArray<FVoxelSectionKey> Keys;
				for (const auto& V : P->Revisions)
				{
					LastSent.Remove(V.Key);
					if (Interest.Contains(V.Key))
						Keys.Add(V.Key);
				}
				if (P->bAtomic && !Keys.IsEmpty())
				{
					if (DirtyTransactions.Num() >= 64)
					{
						Fail(TEXT("Atomic retry backlog exceeded"));
						return;
					}
					DirtyTransactions.Insert(MoveTemp(Keys), 0);
				}
			}
			PendingBatches.Remove(ID);
		}
		if (Now - LastInterest > .25)
		{
			RefreshInterest();
			LastInterest = Now;
		}
		if (InventoryPawn != PC->GetPawn())
		{
			InventoryPawn = PC->GetPawn();
			++InventoryEpoch;
			LastInventory = -10;
		}
		if (Now - LastInventory > 1)
		{
			SendInventory();
			LastInventory = Now;
		}
		if (PendingBatches.Num() < 4 && Transfer.QueuedBytes() < 2 * 1024 * 1024)
		{
			if (!DirtyTransactions.IsEmpty())
			{
				auto Keys = DirtyTransactions[0];
				if (QueueSnapshots(Keys))
					DirtyTransactions.RemoveAt(0);
			}
			else
			{
				TArray<FVoxelSectionKey> Keys;
				for (const auto& K : Interest)
				{
					FVoxelSectionOverlay O;
					if (!M->CopyOverlay(K, O))
						continue;
					const uint64* Known = Acknowledged.Find(K);
					if ((!Known || *Known != O.Revision) && Now - LastSent.FindRef(K) > 3)
					{
						Keys.Add(K);
						if (Keys.Num() == 16)
							break;
					}
				}
				if (!Keys.IsEmpty())
					QueueSnapshots(Keys, false);
			}
		}
	}
	else
	{
		if (!PC->IsLocalController() || !M->GetRegistry())
			return;
		if (!bWelcomed && Now - LastHello > .5)
		{
			FVoxelByteWriter W(64);
			W.Guid(Nonce);
			W.U64(M->GetRegistry()->Hash);
			TArray<uint8> B;
			W.Finish(B);
			Send(EVoxelMessage::Hello, B);
			LastHello = Now;
		}
		if (bReady && Now - LastInterest > .5)
		{
			SendFineInterest();
			TArray<FVoxelSectionKey> Missing;
			if (auto* R = M->GetRuntime())
				for (const auto& K : R->ResidentKeys())
					if (R->Find(K)->Status != EVoxelSectionStatus::DataReady)
					{
						Missing.Add(K);
						if (Missing.Num() == 16)
							break;
					}
			if (!Missing.IsEmpty())
			{
				FVoxelByteWriter Q(512);
				Q.U8(uint8(Missing.Num()));
				for (const auto& K : Missing)
					FVoxelNetworkCodec::WriteKey(Q, K);
				TArray<uint8> P;
				Q.Finish(P);
				Send(EVoxelMessage::Resync, P);
			}
			LastInterest = Now;
		}
		if (!PendingInventory.IsEmpty() && InventoryPawn == PC->GetPawn() && PendingInventoryEpoch == InventoryEpoch)
		{
			auto B = MoveTemp(PendingInventory);
			PendingInventory.Reset();
			if (!ApplyInventory(B))
				Fail(TEXT("Owner inventory layout differs"));
		}
	}
}
void UVoxelModuleNetworkComponent::ServerReceive_Implementation(const FVoxelRPCPacket& P)
{
	Receive(P, true);
}
void UVoxelModuleNetworkComponent::ClientReceive_Implementation(const FVoxelRPCPacket& P)
{
	Receive(P, false);
}
void UVoxelModuleNetworkComponent::Receive(const FVoxelRPCPacket& P, bool Server)
{
	if (bRejected)
		return;
	double Now = FPlatformTime::Seconds();
	if (Now - PacketWindow >= 1)
	{
		PacketWindow = Now;
		PacketsInWindow = 0;
	}
	if (++PacketsInWindow > 256)
	{
		Fail(TEXT("Packet rate limit exceeded"));
		return;
	}
	TArray<uint8> B;
	if (!Transfer.Receive(P.Bytes, Now, B))
	{
		Fail(TEXT("Invalid fragment"));
		return;
	}
	if (B.IsEmpty())
		return;
	FVoxelWireMessage M;
	if (!FVoxelNetworkCodec::Decode(B, M))
	{
		Fail(TEXT("Invalid message envelope"));
		return;
	}
	Handle(M, Server);
}
void UVoxelModuleNetworkComponent::Handle(const FVoxelWireMessage& W, bool Server)
{
	auto* M = Module.Get();
	auto* PC = Controller();
	if (!M || !PC)
		return;
	FVoxelByteReader R(W.Payload);
	if (Server && W.Kind == EVoxelMessage::Hello)
	{
		FGuid N = R.Guid();
		uint64 Hash = R.U64();
		if (!R.End() || !N.IsValid() || !M->GetRegistry() || Hash != M->GetRegistry()->Hash)
		{
			Fail(TEXT("Registry mismatch"));
			return;
		}
		if (bReady)
		{
			Fail(TEXT("Repeated handshake after Ready"));
			return;
		}
		Nonce = N;
		return;
	}
	if (!Server && W.Kind == EVoxelMessage::Welcome)
	{
		FGuid Echo = R.Guid();
		auto B = R.Blob(FVoxelManifestCodec::MaxBytes);
		FVoxelWorldManifest Manifest;
		if (!R.End() || Echo != Nonce || !W.Session.IsValid() || !FVoxelManifestCodec::Decode(B, Manifest))
		{
			Fail(TEXT("Invalid welcome manifest"));
			return;
		}
		FString E;
		if (M->IsReady() && !M->StopWorld(true, E))
		{
			Fail(E);
			return;
		}
		if (!M->StartWorld(Manifest, true, E))
		{
			Fail(E);
			return;
		}
		Session = W.Session;
		bWelcomed = true;
		bReady = true;
		FVoxelByteWriter Q(32);
		Q.U64(Manifest.RecipeHash);
		Q.U64(Manifest.RegistryHash);
		Q.U64(Manifest.BaseSampleHash);
		TArray<uint8> P;
		if (!Q.Finish(P) || !Send(EVoxelMessage::Ready, P))
		{
			Fail(TEXT("Ready message could not be queued"));
		}
		return;
	}
	if (W.Session != Session || !Session.IsValid())
	{
		Fail(TEXT("Stale or missing session"));
		return;
	}
	if (Server && W.Kind == EVoxelMessage::Ready)
	{
		const uint64 Recipe = R.U64();
		const uint64 Registry = R.U64();
		const uint64 Base = R.U64();
		const FVoxelWorldManifest& Manifest = M->GetManifest();
		if (!bWelcomed || !R.End() || Recipe != Manifest.RecipeHash || Registry != Manifest.RegistryHash || Base != Manifest.BaseSampleHash)
		{
			Fail(TEXT("Recipe, registry or base fingerprint mismatch"));
			return;
		}
		bReady = true;
		RefreshInterest();
		SendInventory();
		return;
	}
	if (W.Kind == EVoxelMessage::Closed && !Server)
	{
		FString E = R.String(768);
		Fail(R.End() ? E : TEXT("Server closed the voxel protocol"));
		return;
	}
	if (!bReady)
	{
		Fail(TEXT("Message before Ready"));
		return;
	}
	if (HandlePhase2(W, Server))
		return;
	if (Server && W.Kind == EVoxelMessage::Resync)
	{
		uint8 N = R.U8();
		if (!N || N > 32)
		{
			Fail(TEXT("Invalid resync count"));
			return;
		}
		for (uint8 I = 0; I < N; ++I)
		{
			auto K = FVoxelNetworkCodec::ReadKey(R);
			if (Interest.Contains(K))
			{
				Acknowledged.Remove(K);
				LastSent.Remove(K);
			}
		}
		if (!R.End())
			Fail(TEXT("Invalid resync payload"));
		return;
	}
	if (Server && W.Kind == EVoxelMessage::Ack)
	{
		const FGuid ID = R.Guid();
		const uint8 N = R.U8();
		if (!ID.IsValid() || !N || N > 32)
		{
			Fail(TEXT("Invalid snapshot ACK"));
			return;
		}
		TMap<FVoxelSectionKey, uint64> Entries;
		for (uint8 I = 0; I < N; ++I)
		{
			const auto K = FVoxelNetworkCodec::ReadKey(R);
			const uint64 V = R.U64();
			if (Entries.Contains(K))
			{
				Fail(TEXT("Duplicate ACK key"));
				return;
			}
			Entries.Add(K, V);
		}
		if (!R.End())
		{
			Fail(TEXT("Malformed snapshot ACK"));
			return;
		}
		const auto* Pending = PendingBatches.Find(ID);
		if (!Pending)
			return; // An expired/repeated ACK does not revive an old baseline.
		if (!Pending->bSent || Entries.Num() != Pending->Revisions.Num())
		{
			Fail(TEXT("ACK does not match a sent batch"));
			return;
		}
		for (const auto& E : Entries)
		{
			const uint64* Sent = Pending->Revisions.Find(E.Key);
			if (!Sent || *Sent != E.Value)
			{
				Fail(TEXT("ACK version was not sent"));
				return;
			}
		}
		for (const auto& E : Entries)
			if (Interest.Contains(E.Key))
				Acknowledged.Add(E.Key, FMath::Max(Acknowledged.FindRef(E.Key), E.Value));
		PendingBatches.Remove(ID);
		return;
	}
	if (!Server && W.Kind == EVoxelMessage::Snapshots)
	{
		FString E;
		if (!M->QueueRemoteEncoded(W.Payload, E))
			Fail(E.IsEmpty() ? TEXT("Snapshot backlog exceeded") : E);
		return;
	}
	if (Server && W.Kind == EVoxelMessage::Edit)
	{
		FVoxelEditIntent I;
		if (!FVoxelNetworkCodec::DecodeIntent(W.Payload, I))
		{
			Fail(TEXT("Invalid edit intent"));
			return;
		}
		double Now = FPlatformTime::Seconds();
		EditTokens = FMath::Min(40.0, EditTokens + FMath::Clamp(Now - LastReceiveTick, 0.0, 1.0) * 20);
		LastReceiveTick = Now;
		if (EditTokens < 1 || I.RequestId <= LastRequestId)
		{
			Fail(TEXT("Replay or edit rate limit exceeded"));
			return;
		}
		--EditTokens;
		LastRequestId = I.RequestId;
		AActor* Observer = GetAuthorizedObserver();
		FVoxelEditReply Reply;
		Reply.RequestId = I.RequestId;
		if (Observer && (Observer == PC->GetPawn() || bObserverCanBuild))
			Reply = M->ExecuteIntent(PC, Observer, I, false);
		else
		{
			Reply.Code = EVoxelEditCode::Rejected;
			Reply.Reason = TEXT("Observer has no build permission");
		}
		TArray<uint8> P;
		FVoxelNetworkCodec::EncodeReply(Reply, P);
		Send(EVoxelMessage::Reply, P);
		if (Reply.Code != EVoxelEditCode::Pending)
			SendInventory();
		return;
	}
	if (!Server && W.Kind == EVoxelMessage::Reply)
	{
		FVoxelEditReply Reply;
		if (!FVoxelNetworkCodec::DecodeReply(W.Payload, Reply))
		{
			Fail(TEXT("Invalid edit reply"));
			return;
		}
		OnIntentReply.Broadcast(Reply);
		return;
	}
	if (!Server && W.Kind == EVoxelMessage::Inventory)
	{
		if (!ApplyInventory(W.Payload))
			Fail(TEXT("Invalid owner inventory snapshot"));
		return;
	}
	Fail(TEXT("Message is not permitted in this direction"));
}
AActor* UVoxelModuleNetworkComponent::GetAuthorizedObserver() const
{
	return AuthorizedObserver.IsValid() ? AuthorizedObserver.Get() : (Controller() ? Controller()->GetPawn() : nullptr);
}
void UVoxelModuleNetworkComponent::SetAuthorizedObserver(AActor* A, bool Build)
{
	if (!Controller() || !Controller()->HasAuthority() || A && A->GetWorld() != GetWorld())
		return;
	AuthorizedObserver = A;
	bObserverCanBuild = Build;
	RefreshInterest();
}
void UVoxelModuleNetworkComponent::RefreshInterest()
{
	auto* M = Module.Get();
	auto* PC = Controller();
	AActor* Observer = GetAuthorizedObserver();
	if (!M || !M->IsReady() || !PC || !PC->HasAuthority() || !Observer)
		return;
	FIntVector Center;
	if (!VoxelCoord::FromWorld(Observer->GetActorLocation(), M->BlockSize(), Center))
		return;
	FVoxelStreamingSource S;
	S.Id = SourceId.IsValid() ? SourceId : FGuid::NewGuid();
	S.Center = Center;
	S.Direction = Observer->GetActorForwardVector();
	S.RenderRadius = 0;
	S.CollisionRadius = 2;
	S.SimulationRadius = 0;
	S.PreloadRadius = 2;
	S.VerticalRadius = 2;
	S.bRender = false;
	S.bCollision = true;
	S.bSimulation = false;
	if (SourceId.IsValid())
		M->UpdateSource(SourceId, S);
	else
		SourceId = M->RegisterSource(this, S);
	TSet<FVoxelSectionKey> Next;
	const auto Basic = FVoxelStreaming::Compute({S}, M->GetManifest().Settings);
	for (const auto& P : Basic)
		Next.Add(P.Key);
	const double Size = M->BlockSize();
	const FVector Location = Observer->GetActorLocation();
	for (auto It = Phase2.Fine.CreateIterator(); It; ++It)
	{
		const auto K = *It;
		const FVector Min(double(K.X) * 16 * Size, double(K.Y) * 16 * Size, double(K.Z) * 16 * Size);
		const FVector Max = Min + FVector(16 * Size);
		const double DX = FMath::Max(FMath::Max(Min.X - Location.X, Location.X - Max.X), 0.0);
		const double DY = FMath::Max(FMath::Max(Min.Y - Location.Y, Location.Y - Max.Y), 0.0);
		const double DZ = FMath::Max(FMath::Max(Min.Z - Location.Z, Location.Z - Max.Z), 0.0);
		if (DX * DX + DY * DY > 4800.0 * 4800.0 || DZ > 4800)
		{
			It.RemoveCurrent();
			continue;
		}
		Next.Add(K);
	}
	for (const auto& K : Interest)
		if (!Next.Contains(K))
		{
			Acknowledged.Remove(K);
			LastSent.Remove(K);
		}
	M->SetRemoteFineDemand(this, Phase2.Fine);
	Interest = MoveTemp(Next);
}
bool UVoxelModuleNetworkComponent::QueueSnapshots(const TArray<FVoxelSectionKey>& Keys, bool Atomic)
{
	auto* M = Module.Get();
	if (!M || Keys.IsEmpty() || Keys.Num() > 32 || PendingBatches.Num() >= 4)
		return false;
	FVoxelSnapshotBatch B;
	B.Id = FGuid::NewGuid();
	FPendingSnapshot Pending;
	Pending.CreatedAt = FPlatformTime::Seconds();
	Pending.bAtomic = Atomic;
	uint64 Bound = 17;
	for (const auto& K : Keys)
		if (Interest.Contains(K) && !Pending.Revisions.Contains(K))
		{
			FVoxelSectionOverlay O;
			if (!M->CopyOverlay(K, O))
				return false;
			const uint64 Size = FVoxelDeltaCodec::MaxEncodedBytes(*M->GetRegistry(), O);
			if (!Size)
			{
				Fail(TEXT("Section exceeds the snapshot byte budget"));
				return false;
			}
			if (Bound + 4 + Size > FVoxelDeltaCodec::MaxAtomicBatchWireBytes)
			{
				if (Atomic)
				{
					Fail(TEXT("Pending atomic snapshot outgrew its byte budget; reconnect required"));
					return false;
				}
				break; // Ordinary initial/resync snapshots may be sent in several bounded batches.
			}
			Bound += 4 + Size;
			Pending.Revisions.Add(K, O.Revision);
			B.Sections.Add(MoveTemp(O));
		}
	if (B.Sections.IsEmpty())
		return true;
	if (!M->QueueNetworkEncode(B, this))
		return false;
	PendingBatches.Add(B.Id, MoveTemp(Pending));
	return true;
}
void UVoxelModuleNetworkComponent::CompleteSnapshotSend(FGuid ID, TArray<uint8>&& B, bool OK)
{
	auto* Pending = PendingBatches.Find(ID);
	if (!Pending)
		return;
	if (!OK)
	{
		PendingBatches.Remove(ID);
		Fail(TEXT("Snapshot encoding failed"));
		return;
	}
	if (!Transfer.Enqueue(MoveTemp(B)))
	{
		TArray<FVoxelSectionKey> Keys;
		for (const auto& P : Pending->Revisions)
		{
			LastSent.Remove(P.Key);
			if (Interest.Contains(P.Key))
				Keys.Add(P.Key);
		}
		const bool Atomic = Pending->bAtomic;
		PendingBatches.Remove(ID);
		if (Atomic && !Keys.IsEmpty())
		{
			if (DirtyTransactions.Num() >= 64)
			{
				Fail(TEXT("Atomic retry backlog exceeded"));
				return;
			}
			DirtyTransactions.Insert(MoveTemp(Keys), 0);
		}
		return;
	}
	Pending->bSent = true;
	Pending->SentAt = FPlatformTime::Seconds();
	for (const auto& P : Pending->Revisions)
		LastSent.Add(P.Key, Pending->SentAt);
}
void UVoxelModuleNetworkComponent::OnCommit(const FVoxelEditBatch& B)
{
	NotifyProxyEdit(B);
	if (!Controller() || !Controller()->HasAuthority() || Controller()->IsLocalController() || !bReady)
		return;
	TArray<FVoxelSectionKey> Keys;
	for (const auto& S : B.Sections)
		if (Interest.Contains(S.Key))
			Keys.Add(S.Key);
	if (Keys.IsEmpty())
		return;
	if (DirtyTransactions.Num() >= 64)
	{
		Fail(TEXT("Connection cannot keep up with world edits"));
		return;
	}
	DirtyTransactions.Add(MoveTemp(Keys));
}
void UVoxelModuleNetworkComponent::OnRemoteCompleted(const FVoxelSnapshotBatch& B, bool OK)
{
	if (!Controller() || Controller()->HasAuthority() || !bReady || B.Sections.IsEmpty())
		return;
	FVoxelByteWriter W(2048);
	if (OK)
	{
		W.Guid(B.Id);
		W.U8(uint8(B.Sections.Num()));
		for (const auto& S : B.Sections)
		{
			FVoxelNetworkCodec::WriteKey(W, S.Key);
			W.U64(S.Revision);
		}
	}
	else
	{
		W.U8(uint8(B.Sections.Num()));
		for (const auto& S : B.Sections)
			FVoxelNetworkCodec::WriteKey(W, S.Key);
	}
	TArray<uint8> P;
	if (W.Finish(P))
		Send(OK ? EVoxelMessage::Ack : EVoxelMessage::Resync, P);
}
bool UVoxelModuleNetworkComponent::SubmitIntent(const FVoxelEditIntent& I)
{
	if (!bReady || bRejected || !Controller())
		return false;
	if (Controller()->HasAuthority())
	{
		auto* M = Module.Get();
		if (!M)
			return false;
		auto Reply = M->ExecuteIntent(Controller(), GetAuthorizedObserver(), I, false);
		OnIntentReply.Broadcast(Reply);
		return true;
	}
	TArray<uint8> P;
	return FVoxelNetworkCodec::EncodeIntent(I, P) && Send(EVoxelMessage::Edit, P);
}
void UVoxelModuleNetworkComponent::SendInventory()
{
	auto* PC = Controller();
	if (!PC || !PC->HasAuthority() || PC->IsLocalController() || !bReady)
		return;
	auto* Agent = Cast<IAbilityInventoryAgentInterface>(PC->GetPawn());
	auto* Inv = Agent ? Agent->GetInventory() : nullptr;
	if (!Inv)
		return;
	if (InventoryPawn != PC->GetPawn())
	{
		InventoryPawn = PC->GetPawn();
		++InventoryEpoch;
	}
	auto Slots = Inv->GetAllSlots();
	if (Slots.Num() > 256)
	{
		Fail(TEXT("Inventory bridge supports at most256 slots"));
		return;
	}
	FVoxelByteWriter W(64 * 1024);
	W.U64(InventoryEpoch);
	W.U16(uint16(Slots.Num()));
	for (auto* S : Slots)
	{
		if (!S)
			return;
		const auto& I = S->GetItem();
		W.U8(uint8(S->GetSplitType()));
		W.U16(uint16(S->GetSlotIndex()));
		W.String(I.Count > 0 ? I.ID.ToString() : FString(), 256);
		W.I32(FMath::Max(0, I.Count));
		W.I32(I.Count > 0 ? I.Level : 0);
	}
	TArray<uint8> P;
	if (W.Finish(P))
		Send(EVoxelMessage::Inventory, P);
}
bool UVoxelModuleNetworkComponent::ApplyInventory(const TArray<uint8>& B)
{
	if (B.Num() > 64 * 1024 || !Controller() || Controller()->HasAuthority())
		return false;
	FVoxelByteReader R(B);
	uint64 E = R.U64();
	uint16 N = R.U16();
	if (!E || N > 256)
		return false;
	if (E < InventoryEpoch)
		return true;
	if (E != InventoryEpoch || InventoryPawn != Controller()->GetPawn())
	{
		if (E >= PendingInventoryEpoch)
		{
			PendingInventory = B;
			PendingInventoryEpoch = E;
		}
		return true;
	}
	auto* A = Cast<IAbilityInventoryAgentInterface>(InventoryPawn);
	auto* Inv = A ? A->GetInventory() : nullptr;
	if (!Inv)
	{
		PendingInventory = B;
		PendingInventoryEpoch = E;
		return true;
	}
	struct FEntry
	{
		UAbilityInventorySlotBase* Slot;
		FAbilityItem Item;
	};
	TArray<FEntry> Entries;
	TSet<uint32> Seen;
	for (uint16 I = 0; I < N; ++I)
	{
		uint8 Split = R.U8();
		uint16 Index = R.U16();
		FString ID = R.String(256);
		int32 Count = R.I32(), Level = R.I32();
		uint32 Key = (uint32(Split) << 16) | Index;
		auto* S = Inv->GetSlotBySplitTypeAndIndex(ESlotSplitType(Split), Index);
		if (!R.IsValid() || !S || Seen.Contains(Key) || Count < 0 || Count > 100000 || Level < 0 || Level > 10000 ||
		    (Count == 0 && (!ID.IsEmpty() || Level != 0)))
			return false;
		FAbilityItem Item = Count > 0 ? FAbilityItem(FPrimaryAssetId(ID), Count, Level) : FAbilityItem::Empty;
		if (Count > 0 && !Item.ID.IsValid())
			return false;
		Seen.Add(Key);
		Entries.Add({S, Item});
	}
	if (!R.End())
		return false;
	// Owner presentation mirror only. Never grant gameplay abilities on a client from a voxel packet.
	for (auto& E0 : Entries)
	{
		E0.Item.Payload = E0.Slot;
		E0.Item.Handle = E0.Item.Match(E0.Slot->GetItem()) ? E0.Slot->GetItem().Handle : FGameplayAbilitySpecHandle();
		E0.Slot->GetItem() = E0.Item;
	}
	for (auto& E0 : Entries)
		E0.Slot->OnSlotRefresh.Broadcast();
	Inv->OnRefresh.Broadcast();
	return true;
}
void UVoxelModuleNetworkComponent::PushInventoryFor(AActor* A)
{
	auto* P = Cast<APawn>(A);
	auto* PC = P ? Cast<APlayerController>(P->GetController()) : nullptr;
	if (PC)
		if (auto* N = PC->FindComponentByClass<UVoxelModuleNetworkComponent>())
			N->SendInventory();
}
void UVoxelModuleNetworkComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UVoxelModuleNetworkComponent, InventoryPawn);
	DOREPLIFETIME(UVoxelModuleNetworkComponent, InventoryEpoch);
}

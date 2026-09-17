#include "Voxel/VoxelModule.h"
#include "Voxel/Prefabs/Data/VoxelPrefabData.h"
#include "Voxel/Network/VoxelModuleNetworkComponent.h"
#include "Voxel/Network/VoxelNetworkCodec.h"
#include "Voxel/Chunks/VoxelChunk.h"
#include "Voxel/Rendering/VoxelMaterialSet.h"
#include "Voxel/Voxels/Data/VoxelData.h"
#include "Voxel/Voxels/VoxelItemBridge.h"
#include "Voxel/Interaction/VoxelEditTransaction.h"
#include "Voxel/Interaction/VoxelInventoryTransaction.h"
#include "Voxel/Save/VoxelDeltaCodec.h"
#include "Voxel/Save/VoxelSceneColumnCodec.h"
#include "Voxel/Generation/VoxelManifestCodec.h"
#include "Voxel/Generation/Assets/VoxelWorldGenerationProfile.h"
#include "Voxel/Generation/VoxelGenerationBinding.h"
#include "Voxel/Rendering/VoxelWorldView.h"
#include "Voxel/Rendering/VoxelDetailView.h"
#include "Voxel/Agent/VoxelAgentComponent.h"
#include "Voxel/Components/VoxelCollisionComponent.h"
#include "Voxel/Save/VoxelBlockEntityCodec.h"
#include "Asset/AssetModuleStatics.h"
#include "Ability/AbilityModuleStatics.h"
#include "Ability/Inventory/AbilityInventoryBase.h"
#include "Ability/Inventory/AbilityInventoryAgentInterface.h"
#include "Ability/Inventory/Slot/AbilityInventorySlotBase.h"
#include "Ability/Item/AbilityItemDataBase.h"
#include "Ability/PickUp/AbilityPickUpVoxel.h"
#include "Event/EventModuleStatics.h"
#include "Event/Events/Voxel/Event_VoxelWorldModeChanged.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Main/MainModule.h"
#include "Scene/SceneModule.h"
#include "SaveGame/SaveGameStorage.h"
#include "SaveGame/SaveGameModule.h"
#include "Engine/World.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
IMPLEMENTATION_MODULE(UVoxelModule)
UWorld* UVoxelModule::GetWorld() const
{
	if (HasAnyFlags(RF_ClassDefaultObject))
		return nullptr;
	auto* M = GetTypedOuter<AMainModule>();
	return M ? M->GetWorld() : nullptr;
}
UVoxelModule::UVoxelModule()
{
	ModuleName = TEXT("VoxelModule");
	ModuleDisplayName = FText::FromString(TEXT("Voxel Module"));
	SaveScope = ESaveScope::World;
	SaveDataVersion = 2;
	ModuleDependencies = {FName(TEXT("AbilityModule")), FName(TEXT("SceneModule"))};
	ChunkSpawnClass = UVoxelChunk::StaticClass();
	// RPC bridge is a stable default subobject on AWHPlayerController, not independently created on both peers.
	ModuleNetworkComponent = nullptr;
	WorldData = MakeUnique<FVoxelModuleSaveData>(WorldBasicData);
}
UVoxelModule::~UVoxelModule()
{
	if (Scheduler)
		Scheduler->StopAndJoin();
	DetailView.Reset();
	WorldView.Reset();
	TERMINATION_MODULE(UVoxelModule)
}

#if WITH_EDITOR
void UVoxelModule::OnDestroy()
{
	Super::OnDestroy();

	TERMINATION_MODULE(UVoxelModule)
}
#endif

bool UVoxelModule::IsAuthority() const
{
	return GetWorld() && GetWorld()->GetNetMode() != NM_Client;
}
bool UVoxelModule::IsSaveEnabled() const
{
	return Super::IsSaveEnabled() && !bWorldLoadRejected && IsAuthority() && WorldMode == EVoxelWorldMode::Default;
}

void UVoxelModule::SetWorldMode(EVoxelWorldMode InWorldMode)
{
	if (WorldMode == InWorldMode)
	{
		return;
	}
	WorldMode = InWorldMode;
	UEventModuleStatics::BroadcastEvent<FEventVoxelWorldModeChanged>(this, {WorldMode});
}
void UVoxelModule::OnInitialize()
{
	Super::OnInitialize();
	FString Error;
	bool Rendering = GetWorld() && GetWorld()->GetNetMode() != NM_DedicatedServer;
	auto Assets = UAssetModuleStatics::LoadPrimaryAssets<UVoxelData>(FName(TEXT("Voxel")));
	if (!Registry.Build(Assets, Rendering, Error))
	{
		WorldState = EVoxelWorldState::Failed;
		UE_LOG(LogTemp, Error, TEXT("Voxel registry: %s"), *Error);
		return;
	}
	auto S = MakeShared<FVoxelShapeRegistry, ESPMode::ThreadSafe>();
	S->BuildDefaults();
	Shapes = S;
	if (Rendering)
	{
		MaterialSet = MaterialSetAsset.LoadSynchronous();
		if (!MaterialSet || !MaterialSet->Validate(Error))
		{
			WorldState = EVoxelWorldState::Failed;
			UE_LOG(LogTemp, Error, TEXT("Voxel material set: %s"), *Error);
		}
	}
	if (WorldState != EVoxelWorldState::Failed)
	{
		WorldGenerationProfile = WorldGenerationProfileAsset.LoadSynchronous();
		FVoxelGenerationRuntimeConfig Probe;
		if (!WorldGenerationProfile || !FVoxelGenerationBinding::Build(
				*WorldGenerationProfile, *Registry.GetSnapshot(),
				WorldGenerationProfile->Defaults,
				WorldGenerationProfile->TargetCellCentimeters, Probe, Error))
		{
			WorldState = EVoxelWorldState::Failed;
			UE_LOG(LogTemp, Error, TEXT("Voxel phase2 profile: %s"), *Error);
		}
	}
}
void UVoxelModule::OnPreparatory(EPhase P)
{
	Super::OnPreparatory(P);
	if (bAutoGenerate && IsAuthority() && !Runtime && WorldState != EVoxelWorldState::Failed)
	{
		FString E;
		CreateWorldFromProfile(WorldBasicData.Generation.Seed, E);
	}
}
bool UVoxelModule::CreateWorldFromProfile(int32 Seed, FString& E)
{
	if (!WorldGenerationProfile)
	{
		E = TEXT("No baked world generation profile");
		return false;
	}
	FVoxelGenerationSettings Settings = WorldGenerationProfile->Defaults;
	Settings.Seed = Seed;
	return CreateWorld(Settings, WorldGenerationProfile->TargetCellCentimeters, E);
}
bool UVoxelModule::CreateWorld(const FVoxelGenerationSettings& S, int32 Size, FString& E)
{
	if (!IsAuthority() || !Registry.GetSnapshot() || !WorldGenerationProfile)
	{
		E = TEXT("Authority and initialized generation assets required");
		return false;
	}
	if (Runtime)
	{
		E = TEXT("Close the current world explicitly before creating another");
		return false;
	}
	FVoxelGenerationRuntimeConfig C;
	if (!FVoxelGenerationBinding::Build(*WorldGenerationProfile, *Registry.GetSnapshot(), S, Size, C, E))
		return false;
	FVoxelWorldManifest M;
	M.WorldId = FGuid::NewGuid();
	M.Settings = S;
	M.BlockSizeCentimeters = Size;
	M.RegistryHash = Registry.GetSnapshot()->Hash;
	M.CatalogHash = C.CatalogHash;
	M.RecipeHash = FVoxelManifestCodec::RecipeFingerprint(M);
	FVoxelGenerationPipeline G(C);
	if (!G.BuildHandshakeSignature(M.BaseSampleHash))
	{
		E = TEXT("Generation signature evaluation failed");
		return false;
	}
	TArray<uint8> B;
	if (!FVoxelManifestCodec::Encode(M, B))
	{
		E = TEXT("Invalid new-world manifest");
		return false;
	}
	RegionStore.Reset();
	return StartWorld(M, false, E);
}
bool UVoxelModule::StartWorld(const FVoxelWorldManifest& M, bool FromServer, FString& E)
{
	if (!Registry.GetSnapshot() || !Shapes || !GetWorld() || !WorldGenerationProfile)
	{
		E = TEXT("Voxel phase2 assets are not initialized");
		return false;
	}
	if ((GetWorld()->GetNetMode() == NM_Client) != FromServer || Runtime)
	{
		E = TEXT("World mode mismatch or another world is already active");
		return false;
	}
	if (!M.WorldId.IsValid() || M.RecipeHash != FVoxelManifestCodec::RecipeFingerprint(M) ||
		M.RegistryHash != Registry.GetSnapshot()->Hash || M.CatalogHash != WorldGenerationProfile->CatalogHash)
	{
		E = TEXT("World/registry/generation-content fingerprint mismatch");
		return false;
	}
	FVoxelGenerationRuntimeConfig C;
	if (!FVoxelGenerationBinding::Build(*WorldGenerationProfile, *Registry.GetSnapshot(),
		M.Settings, M.BlockSizeCentimeters, C, E))
		return false;
	auto G = MakeShared<FVoxelGenerationPipeline, ESPMode::ThreadSafe>(C);
	uint64 Signature = 0;
	if (!G->BuildHandshakeSignature(Signature) || Signature != M.BaseSampleHash)
	{
		E = TEXT("Base generation signature mismatch");
		return false;
	}
	if (Epoch == MAX_uint64)
	{
		E = TEXT("World epoch exhausted");
		return false;
	}
	Manifest = M;
	Generator = G;
	Runtime = MakeUnique<FVoxelWorldRuntime>(++Epoch, IsAuthority(), Registry.GetSnapshot().ToSharedRef(), G);
	Scheduler = MakeUnique<FVoxelTaskScheduler>();
	SessionId = IsAuthority() ? FGuid::NewGuid() : FGuid();
	Desired.Reset();
	OrderedDesired.Reset();
	RetryAfter.Reset();
	RetryCount.Reset();
	RemotePending.Reset();
	Breaking.Reset();
	RemoteFineDemand.Reset();
	LastStreaming = -1;
	bRemoteRunning = false;
	if (FromServer)
		RegionStore.Reset();
	bWorldLoadRejected = false;
	WorldState = EVoxelWorldState::Running;
	if (!WorldData)
		WorldData = NewWorldData();
	WorldData->Generation = M.Settings;
	WorldData->BlockSizeCentimeters = M.BlockSizeCentimeters;
	if (!FVoxelManifestCodec::Encode(M, WorldData->ManifestBytes))
	{
		E = TEXT("World manifest could not be stored");
		WorldState = EVoxelWorldState::Failed;
		return false;
	}
	WorldView = MakeUnique<FVoxelWorldView>(*this, *Scheduler, RegionStore, Epoch,
		GetWorld()->GetNetMode() != NM_DedicatedServer);
	DetailView = MakeUnique<FVoxelDetailView>(*this, *Scheduler, Epoch);
	if (!DetailView->Initialize(E))
	{
		Scheduler->StopAndJoin();
		DetailView.Reset();
		WorldView.Reset();
		Runtime.Reset();
		Generator.Reset();
		Scheduler.Reset();
		WorldState = EVoxelWorldState::Failed;
		return false;
	}
	OnWorldInitialized.Broadcast();
	E.Reset();
	return true;
}
bool UVoxelModule::StopWorld(bool Discard, FString& E)
{
	if (SaveAdapter.IsBusy())
	{
		E = TEXT("Finish the SaveGame transaction before closing the voxel world");
		return false;
	}
	if (Runtime && !Discard)
	{
		for (const auto& K : Runtime->ResidentKeys())
			if (Runtime->Find(K)->IsSaveDirty())
			{
				E = TEXT("World contains uncommitted voxel edits");
				return false;
			}
		if (!UnloadedSceneFiles.IsEmpty())
		{
			E = TEXT("World contains uncommitted scene actors");
			return false;
		}
	}
	WorldState = EVoxelWorldState::Closing;
	if (Scheduler)
		Scheduler->StopAndJoin();
	DetailView.Reset();
	WorldView.Reset();
	RemoteFineDemand.Reset();
	Scheduler.Reset();
	for (auto& P : Columns)
		if (P.Value)
			P.Value->Shutdown();
	Columns.Reset();
	Runtime.Reset();
	Generator.Reset();
	RemotePending.Reset();
	EncodedRemotePending.Reset();
	NetworkRecipients.Reset();
	NetworkBatchIds.Reset();
	RemoteInFlight.Reset();
	bDecodeRunning = bRemoteRunning = false;
	Desired.Reset();
	Breaking.Reset();
	RetryAfter.Reset();
	RetryCount.Reset();
	UnloadedSceneFiles.Reset();
	CapturedSceneFiles.Reset();
	SessionId.Invalidate();
	WorldState = EVoxelWorldState::None;
	E.Reset();
	return true;
}
void UVoxelModule::OnTermination(EPhase P)
{
	// Ordinary quit UI first completes SaveActiveSlotAsync. Engine teardown is not an implicit successful save.
	if (SaveAdapter.IsBusy())
		if (USaveGameModule* SaveGameModule = USaveGameModule::GetPtr())
			SaveGameModule->FinishPendingSave();
	FString E;
	if (!StopWorld(true, E))
		UE_LOG(LogTemp, Error, TEXT("Voxel close failed: %s"), *E);
	Super::OnTermination(P);
}
void UVoxelModule::OnRefresh(float Dt, bool InEditor)
{
	Super::OnRefresh(Dt, InEditor);
	if (InEditor || !IsReady())
		return;
	SectionAllocationsThisFrame = 0;
	Scheduler->Tick(
	    [this](FVoxelTaskResult&& R)
	    {
			ApplyTask(MoveTemp(R));
		});
	if (WorldView)
		WorldView->Tick(CollectLocalViewObservers());
	if (DetailView)
		DetailView->Tick(CollectDetailObservers());
	double Now = FPlatformTime::Seconds();
	if (LastStreaming < 0 || Now - LastStreaming >= .2)
	{
		RefreshStreaming(Now);
		LastStreaming = Now;
	}
	// Dispatch backlog every frame, not only when the source changes.
	for (int32 I = 0; I < FMath::Min(32, OrderedDesired.Num()); ++I)
		if (const auto* D = Desired.Find(OrderedDesired[I]))
			QueueSection(OrderedDesired[I], *D);
	for (int32 I = 0; I < 96 && !OrderedDesired.IsEmpty(); ++I)
	{
		DispatchCursor %= OrderedDesired.Num();
		auto K = OrderedDesired[DispatchCursor++];
		if (const auto* D = Desired.Find(K))
			QueueSection(K, *D);
	}
	PumpRemote();
	const double SceneEnd = FPlatformTime::Seconds() + .0005;
	for (int32 I = 0; I < 4 && !SceneColumnOrder.IsEmpty() && FPlatformTime::Seconds() < SceneEnd; ++I)
	{
		SceneTickCursor %= SceneColumnOrder.Num();
		const FIntPoint C = SceneColumnOrder[SceneTickCursor++];
		auto* Column = GetColumn(C);
		if (!Column || !Column->bSceneReady || Column->bSceneFailed)
			continue;
		bool Sim = false;
		for (int32 Z = VoxelCoord::FloorDiv(Manifest.Settings.MinZ, 16); Z <= VoxelCoord::FloorDiv(Manifest.Settings.MaxZ - 1, 16); ++Z)
			if (const auto* D = Desired.Find({C.X, C.Y, Z}))
				if (D->bSimulation)
				{
					Sim = true;
					break;
				}
		if (Sim)
			Column->TickSceneActors(Dt);
	}
	for (auto It = Breaking.CreateIterator(); It; ++It)
		if (!It.Key().IsValid() || Now - It.Value().LastPulse > .35)
			It.RemoveCurrent();
}
FGuid UVoxelModule::RegisterSource(UObject* Owner, const FVoxelStreamingSource& S)
{
	if (!Owner || Owner->GetWorld() != GetWorld())
		return FGuid();
	FVoxelStreamingSource V = S;
	V.Id = FGuid::NewGuid();
	if (!FVoxelStreaming::Validate(V))
		return FGuid();
	FSource N;
	N.Owner = Owner;
	N.Value = V;
	Sources.Add(V.Id, N);
	LastStreaming = -1;
	return V.Id;
}
bool UVoxelModule::UpdateSource(const FGuid& ID, const FVoxelStreamingSource& S)
{
	auto* N = Sources.Find(ID);
	if (!N)
		return false;
	auto V = S;
	V.Id = ID;
	if (!FVoxelStreaming::Validate(V))
		return false;
	if (VoxelCoord::Section(V.Center) != VoxelCoord::Section(N->Value.Center))
		LastStreaming = -1;
	N->Value = V;
	return true;
}
void UVoxelModule::UnregisterSource(const FGuid& ID)
{
	Sources.Remove(ID);
	LastStreaming = -1;
}
TArray<FVector> UVoxelModule::CollectLocalViewObservers() const
{
	TArray<FVector> Result;
	TSet<FIntVector> Seen;
	for (const auto& Pair : Sources)
	{
		const auto& S = Pair.Value;
		if (!S.Owner.IsValid() || !S.Value.bRender || Seen.Contains(S.Value.Center))
			continue;
		Seen.Add(S.Value.Center);
		Result.Add(FVector(S.Value.Center) * BlockSize());
	}
	return Result;
}
TArray<FVector> UVoxelModule::CollectDetailObservers() const
{
	TArray<FVector> Result;
	TSet<FIntVector> Seen;
	for (const auto& Pair : Sources)
	{
		const auto& S = Pair.Value;
		if (!S.Owner.IsValid() || !(S.Value.bRender || S.Value.bCollision) || Seen.Contains(S.Value.Center))
			continue;
		Seen.Add(S.Value.Center);
		Result.Add(FVector(S.Value.Center) * BlockSize());
	}
	return Result;
}
void UVoxelModule::SetRemoteFineDemand(UObject* Owner, const TSet<FVoxelSectionKey>& Keys)
{
	if (!IsAuthority() || !Owner || Owner->GetWorld() != GetWorld() || Keys.Num() > 8192)
		return;
	RemoteFineDemand.Add(Owner, Keys);
	LastStreaming = -1;
}
void UVoxelModule::ClearRemoteFineDemand(UObject* Owner)
{
	if (RemoteFineDemand.Remove(Owner))
		LastStreaming = -1;
}
void UVoxelModule::GetFineViewKeys(TArray<FVoxelSectionKey>& Out) const
{
	TArray<FVoxelStreamingSource> Local;
	for (const auto& P : Sources)
		if (P.Value.Owner.IsValid() && P.Value.Value.bRender)
			Local.Add(P.Value.Value);
	auto Needed = FVoxelStreaming::Compute(Local, Manifest.Settings);
	if (WorldView)
		WorldView->AppendFineDemand(Needed);
	Out.Reset();
	for (const auto& P : Needed)
		if (VoxelCoord::IsValidSection(P.Key, Manifest.Settings.MinZ, Manifest.Settings.MaxZ))
			Out.Add(P.Key);
	Out.Sort([](const auto& A, const auto& B) { return A < B; });
}
UVoxelChunk* UVoxelModule::GetColumn(FIntPoint K, bool Create)
{
	if (const TObjectPtr<UVoxelChunk> Column = Columns.FindRef(K))
	{
		return Column.Get();
	}
	if (!Create || !Runtime)
		return nullptr;
	auto* C = NewObject<UVoxelChunk>(this, ChunkSpawnClass);
	if (!C->Initialize(this, K))
		return nullptr;
	Columns.Add(K, C);
	return C;
}
AActor* UVoxelModule::FindSceneActor(const FGuid& ID) const
{
	for (const auto& P : Columns)
		if (P.Value)
			if (AActor* A = P.Value->GetSceneActors().FindRef(ID))
				if (::IsValid(A))
					return A;
	return nullptr;
}
void UVoxelModule::RefreshStreaming(double Now)
{
	TArray<FVoxelStreamingSource> A;
	for (auto It = Sources.CreateIterator(); It; ++It)
	{
		if (!It.Value().Owner.IsValid())
		{
			It.RemoveCurrent();
			continue;
		}
		A.Add(It.Value().Value);
	}
	Columns.GetKeys(SceneColumnOrder);
	Desired = FVoxelStreaming::Compute(A, Manifest.Settings);
	if (WorldView)
	{
		for (auto& P : Desired)
			P.Value.bMesh = false;
		WorldView->AppendFineDemand(Desired);
	}
	for (auto It = RemoteFineDemand.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
			continue;
		}
		for (const auto& K : It.Value())
		{
			auto& Demand = Desired.FindOrAdd(K);
			Demand.bMesh = false;
			Demand.Priority = FMath::Min(Demand.Priority, -500.0);
		}
	}
	for (auto It = Desired.CreateIterator(); It; ++It)
		if (!VoxelCoord::IsValidSection(It.Key(), Manifest.Settings.MinZ, Manifest.Settings.MaxZ))
			It.RemoveCurrent();
	auto Keys = FVoxelStreaming::ByPriority(Desired);
	OrderedDesired = Keys;
	for (const auto& K : Keys)
	{
		auto* S = Runtime->Find(K);
		if (!S)
			continue;
		const auto& D = Desired.FindChecked(K);
		S->LastWanted = Now;
		if (S->bWantsMesh && !D.bMesh)
			if (auto* C = GetColumn({K.X, K.Y}))
				C->ClearMesh(K.Z);
		if (S->bWantsCollision && !D.bCollision)
		{
			S->bHasCollision = false;
			if (auto* C = GetColumn({K.X, K.Y}))
				C->ClearCollision(K.Z);
		}
		if (!S->bWantsMesh && D.bMesh)
			S->bMeshDirty = true;
		if (!S->bWantsCollision && D.bCollision)
			S->bCollisionDirty = true;
		S->bWantsMesh = D.bMesh;
		S->bWantsCollision = D.bCollision;
		S->bWantsSimulation = D.bSimulation;
	}
	auto Resident = Runtime->ResidentKeys();
	int32 Removed = 0;
	for (const auto& K : Resident)
	{
		if (Removed >= 16)
			break;
		auto* S = Runtime->Find(K);
		if (!S || Desired.Contains(K) || S->PinCount || S->IsSaveDirty() || Now - S->LastWanted < 2)
			continue;
		bool Last = true;
		for (const auto& Other : Resident)
			if (Other != K && Other.X == K.X && Other.Y == K.Y && Runtime->Find(Other))
			{
				Last = false;
				break;
			}
		UVoxelChunk* C = GetColumn({K.X, K.Y});
		FString E;
		if (Last && C && IsAuthority() && C->bSceneReady && !CaptureColumnForUnload(*C, E))
			continue;
		Scheduler->CancelSection(K);
		if (!Runtime->Remove(K))
			continue;
		++Removed;
		RetryCount.Remove(K);
		RetryAfter.Remove(K);
		if (C)
		{
			C->ClearMesh(K.Z);
			C->ClearCollision(K.Z);
			if (Last)
			{
				C->Shutdown();
				Columns.Remove({K.X, K.Y});
			}
		}
	}
}
void UVoxelModule::QueueSection(const FVoxelSectionKey& K, const FVoxelSectionDemand& D)
{
	const double Now = FPlatformTime::Seconds();
	auto* S = Runtime->Find(K);
	if (!S && SectionAllocationsThisFrame < 16 && Runtime->NumSections() < MaxResidentSections)
	{
		S = Runtime->Allocate(K, Now);
		if (S)
		{
			++SectionAllocationsThisFrame;
			S->LastWanted = Now;
			S->bWantsMesh = D.bMesh;
			S->bWantsCollision = D.bCollision;
			S->bWantsSimulation = D.bSimulation;
		}
	}
	if (!S || RetryCount.FindRef(K) >= 3)
		return;
	if (RetryAfter.FindRef(K) > Now)
		return;
	if (S->Status == EVoxelSectionStatus::Failed)
	{
		if (RetryCount.FindRef(K) >= 3)
			return;
		S->Status = EVoxelSectionStatus::Allocated;
	}
	if (S->Status == EVoxelSectionStatus::Allocated)
	{
		if (!IsAuthority() || Scheduler->Has(S->Stamp, EVoxelTaskKind::Generate))
			return;
		FVoxelTaskRequest Q;
		Q.Stamp = S->Stamp;
		Q.Kind = EVoxelTaskKind::Generate;
		Q.Priority = D.Priority;
		Q.ReservedBytes = 2 * 1024 * 1024;
		auto Stamp = S->Stamp;
		auto G = Generator;
		auto R = Registry.GetSnapshot();
		auto M = Manifest;
		auto Read = RegionStore.CaptureRead(K);
		Q.Execute = [Stamp, G, R, M, Read](const std::atomic_bool& Cancel)
		{
			FVoxelTaskResult O;
			O.Stamp = Stamp;
			O.Kind = EVoxelTaskKind::Generate;
			O.Overlay.Key = Stamp.Key;
			if (!G->GenerateSection(Stamp.Key, O.Base, &Cancel))
			{
				O.bCanceled = Cancel.load();
				return O;
			}
			TArray<uint8> B;
			auto Status = FVoxelRegionStore::Read(Read, B, O.Error);
			if (Status == EVoxelRegionRead::Failed)
				return O;
			if (Status == EVoxelRegionRead::Loaded && !FVoxelDeltaCodec::Decode(B, M, *R, O.Overlay))
			{
				O.Error = TEXT("Corrupt section delta");
				return O;
			}
			O.bSuccess = !Cancel.load();
			O.bCanceled = Cancel.load();
			return O;
		};
		Scheduler->Enqueue(MoveTemp(Q));
		return;
	}
	if (S->Status != EVoxelSectionStatus::DataReady)
		return;
	if (D.bMesh && S->bMeshDirty && GetWorld()->GetNetMode() != NM_DedicatedServer && !Scheduler->Has(S->Stamp, EVoxelTaskKind::Mesh))
	{
		FVoxelSectionSnapshot Snap;
		if (Runtime->CaptureSnapshot(K, Snap))
		{
			FVoxelTaskRequest Q;
			Q.Stamp = S->Stamp;
			Q.Kind = EVoxelTaskKind::Mesh;
			Q.Priority = D.Priority;
			Q.InputBytes = Snap.Bytes();
			Q.ReservedBytes = 48 * 1024 * 1024;
			auto R = Registry.GetSnapshot();
			auto H = Shapes;
			Q.Execute = [Snap = MoveTemp(Snap), R, H](const std::atomic_bool& C)
			{
				FVoxelTaskResult O;
				O.Stamp = Snap.Stamp;
				O.Kind = EVoxelTaskKind::Mesh;
				O.bSuccess = FVoxelSectionMesher::Build(Snap, *R, *H, O.Mesh, &C);
				O.bCanceled = C.load();
				return O;
			};
			Scheduler->Enqueue(MoveTemp(Q));
		}
	}
	if (D.bCollision && S->bCollisionDirty && !Scheduler->Has(S->Stamp, EVoxelTaskKind::Collision))
	{
		FVoxelSectionSnapshot Snap;
		if (Runtime->CaptureSnapshot(K, Snap))
		{
			FVoxelTaskRequest Q;
			Q.Stamp = S->Stamp;
			Q.Kind = EVoxelTaskKind::Collision;
			Q.Priority = D.Priority - 200000;
			Q.InputBytes = Snap.Bytes();
			Q.ReservedBytes = 2 * 1024 * 1024;
			auto R = Registry.GetSnapshot();
			auto H = Shapes;
			Q.Execute = [Snap = MoveTemp(Snap), R, H](const std::atomic_bool& C)
			{
				FVoxelTaskResult O;
				O.Stamp = Snap.Stamp;
				O.Kind = EVoxelTaskKind::Collision;
				O.bSuccess = FVoxelCollisionBuilder::Build(Snap, *R, *H, O.Collision, &C);
				O.bCanceled = C.load();
				return O;
			};
			Scheduler->Enqueue(MoveTemp(Q));
		}
	}
	if (IsAuthority() && D.bSimulation)
	{
		if (auto* C = GetColumn({K.X, K.Y}, true))
			if (!C->bSceneReady && !C->bSceneLoading && !C->bSceneFailed)
				QueueSceneLoad(*C, *S);
	}
}
void UVoxelModule::ApplyTask(FVoxelTaskResult&& R)
{
	if (!Runtime)
		return;
	if (WorldView && WorldView->OnTask(MoveTemp(R)))
		return;
	if (DetailView && DetailView->OnTask(MoveTemp(R)))
		return;
	if (R.Kind == EVoxelTaskKind::EncodeNetwork)
	{
		uint64 ID = R.Stamp.GenerationToken;
		auto Recipient = NetworkRecipients.FindRef(ID);
		FGuid Batch = NetworkBatchIds.FindRef(ID);
		NetworkRecipients.Remove(ID);
		NetworkBatchIds.Remove(ID);
		if (R.Stamp.WorldEpoch == Epoch && Recipient.IsValid())
			Recipient->CompleteSnapshotSend(Batch, MoveTemp(R.Bytes), R.bSuccess && !R.bCanceled);
		return;
	}
	if (R.Kind == EVoxelTaskKind::DecodeNetwork)
	{
		bDecodeRunning = false;
		if (R.Stamp.WorldEpoch != Epoch)
			return;
		if (!R.bSuccess || R.bCanceled)
		{
			UE_LOG(LogTemp, Error, TEXT("Invalid remote snapshot payload"));
			if (!R.bCanceled)
			{
				WorldState = EVoxelWorldState::Failed;
				LastSaveError = TEXT("Invalid remote snapshot; reconnect required");
			}
			return;
		}
		FVoxelSnapshotBatch B;
		B.Id = R.BatchId;
		B.Sections = MoveTemp(R.RemoteOverlays);
		FString E;
		if (!QueueRemoteBatch(B, E))
			OnRemoteBatchCompleted.Broadcast(B, false);
		return;
	}
	if (R.Kind == EVoxelTaskKind::RebuildOverlay)
	{
		bRemoteRunning = false;
		FVoxelSnapshotBatch B;
		B.Id = R.BatchId;
		B.Sections = R.RemoteOverlays;
		if (RemoteInFlight.IsSet() && !B.Id.IsValid())
			B = RemoteInFlight.GetValue();
		RemoteInFlight.Reset();
		bool OK = R.bSuccess && !R.bCanceled;
		for (const auto& T : R.RemoteStamps)
			if (!Runtime->IsCurrent(T, false))
				OK = false;
		if (OK)
			OK = Runtime->ApplyRemoteSnapshots(R.RemoteOverlays, MoveTemp(R.RemoteBases));
		OnRemoteBatchCompleted.Broadcast(B, OK);
		return;
	}
	if (R.Kind == EVoxelTaskKind::LoadScene)
	{
		auto* C = GetColumn({R.Stamp.Key.X, R.Stamp.Key.Y});
		if (!C)
			return;
		C->bSceneLoading = false;
		if (R.bCanceled || !Runtime->IsCurrent(R.Stamp, false))
			return;
		if (!R.bSuccess)
		{
			C->bSceneFailed = true;
			UE_LOG(LogTemp, Error, TEXT("Voxel scene load: %s"), *R.Error);
			return;
		}
		FString E;
		if (R.Bytes.IsEmpty())
		{
			C->bSceneReady = true;
		}
		else if (!C->RestoreActors(R.Bytes, E))
		{
			C->bSceneFailed = true;
			UE_LOG(LogTemp, Error, TEXT("Voxel scene restore: %s"), *E);
		}
		return;
	}
	if (!Runtime->IsCurrent(R.Stamp, R.Kind != EVoxelTaskKind::Generate))
		return;
	if (!R.bSuccess)
	{
		if (!R.bCanceled)
		{
			int32& N = RetryCount.FindOrAdd(R.Stamp.Key);
			++N;
			RetryAfter.Add(R.Stamp.Key, FPlatformTime::Seconds() + N);
			if (R.Kind == EVoxelTaskKind::Generate)
				Runtime->Find(R.Stamp.Key)->Status = EVoxelSectionStatus::Failed;
			UE_LOG(LogTemp, Error, TEXT("Voxel job %d failed at %d,%d,%d: %s"), int32(R.Kind), R.Stamp.Key.X, R.Stamp.Key.Y, R.Stamp.Key.Z, *R.Error);
		}
		return;
	}
	if (R.Kind == EVoxelTaskKind::Generate)
	{
		if (!Runtime->PublishLoaded(R.Stamp, MoveTemp(R.Base), R.Overlay, true))
		{
			Runtime->Find(R.Stamp.Key)->Status = EVoxelSectionStatus::Failed;
			RetryCount.Add(R.Stamp.Key, 3);
			return;
		}
		if (auto* C = GetColumn({R.Stamp.Key.X, R.Stamp.Key.Y}, true))
			C->OnSectionActivated(R.Stamp.Key);
	}
	else if (auto* C = GetColumn({R.Stamp.Key.X, R.Stamp.Key.Y}, true))
	{
		bool Attempted = false;
		bool Applied = false;
		if (R.Kind == EVoxelTaskKind::Mesh && Runtime->Find(R.Stamp.Key)->bWantsMesh)
		{
			Attempted = true;
			Applied = MaterialSet && C->ApplyMesh(R.Mesh, *MaterialSet, BlockSize());
			if (Applied)
				Runtime->MarkMeshApplied(R.Stamp);
		}
		if (R.Kind == EVoxelTaskKind::Collision && Runtime->Find(R.Stamp.Key)->bWantsCollision)
		{
			Attempted = true;
			Applied = C->ApplyCollision(R.Collision, BlockSize());
			if (Applied)
				Runtime->MarkCollisionApplied(R.Stamp);
		}
		if (Attempted && !Applied)
		{
			int32& N = RetryCount.FindOrAdd(R.Stamp.Key);
			++N;
			RetryAfter.Add(R.Stamp.Key, FPlatformTime::Seconds() + N);
			UE_LOG(LogTemp, Error, TEXT("Voxel component apply failed; section %d,%d,%d attempt %d"), R.Stamp.Key.X, R.Stamp.Key.Y, R.Stamp.Key.Z, N);
		}
	}
}
bool UVoxelModule::QueueRemoteBatch(const FVoxelSnapshotBatch& B, FString& E)
{
	if (!IsReady() || IsAuthority() || RemotePending.Num() >= 4 || B.Sections.IsEmpty() || B.Sections.Num() > 32)
		return false;
	for (const auto& O : B.Sections)
		if (!VoxelCoord::IsValidSection(O.Key, Manifest.Settings.MinZ, Manifest.Settings.MaxZ))
			return false;
	RemotePending.Add(B);
	E.Reset();
	return true;
}
void UVoxelModule::PumpRemote()
{
	PumpDecode();
	if (IsAuthority() || bRemoteRunning || RemotePending.IsEmpty())
		return;
	FVoxelSnapshotBatch B = MoveTemp(RemotePending[0]);
	RemotePending.RemoveAt(0);
	TArray<FVoxelTaskStamp> Stamps;
	bool AnyNew = false;
	for (const auto& O : B.Sections)
	{
		auto* S = Runtime->Find(O.Key);
		if (!S && Runtime->NumSections() < MaxResidentSections)
			S = Runtime->Allocate(O.Key, FPlatformTime::Seconds());
		if (!S || S->Stamp.Revision > O.Revision)
		{
			OnRemoteBatchCompleted.Broadcast(B, false);
			return;
		}
		if (S->Status != EVoxelSectionStatus::DataReady || S->Stamp.Revision < O.Revision)
			AnyNew = true;
		Stamps.Add(S->Stamp);
	}
	if (!AnyNew)
	{
		OnRemoteBatchCompleted.Broadcast(B, true);
		return;
	}
	FVoxelTaskRequest Q;
	Q.Kind = EVoxelTaskKind::RebuildOverlay;
	Q.Stamp = Stamps[0];
	Q.Priority = -300000;
	Q.ReservedBytes = 16 * 1024 * 1024;
	Q.InputBytes = 0;
	for (const auto& O : B.Sections)
	{
		Q.InputBytes += O.Blocks.Num() * 16;
		for (const auto& P : O.Entities)
			Q.InputBytes += P.Value.Payload.Num() + 64;
	}
	auto G = Generator;
	Q.Execute = [B, Stamps, G](const std::atomic_bool& C)
	{
		FVoxelTaskResult O;
		O.Kind = EVoxelTaskKind::RebuildOverlay;
		O.Stamp = Stamps[0];
		O.BatchId = B.Id;
		O.RemoteStamps = Stamps;
		O.RemoteOverlays = B.Sections;
		for (const auto& S : B.Sections)
		{
			FVoxelSectionStorage Base;
			if (!G->GenerateSection(S.Key, Base, &C))
			{
				O.bCanceled = C.load();
				return O;
			}
			O.RemoteBases.Add(MoveTemp(Base));
		}
		O.bSuccess = true;
		return O;
	};
	if (Scheduler->Enqueue(MoveTemp(Q)))
	{
		bRemoteRunning = true;
		RemoteInFlight = B;
	}
	else
		RemotePending.Insert(MoveTemp(B), 0);
}
bool UVoxelModule::CopyOverlay(const FVoxelSectionKey& K, FVoxelSectionOverlay& O) const
{
	const auto* S = Runtime ? Runtime->Find(K) : nullptr;
	if (!S || S->Status != EVoxelSectionStatus::DataReady)
		return false;
	O = S->Overlay;
	return true;
}
FVoxelTraceResult UVoxelModule::Trace(const FVector& S, const FVector& D, double Range) const
{
	if (!IsReady())
		return {};
	return FVoxelRaycast::Trace(*Runtime, *Registry.GetSnapshot(), *Shapes, S, D, Range, BlockSize());
}
bool UVoxelModule::IsActorRayClear(const FVector& S, const FVector& E, AActor* Ignore) const
{
	FCollisionQueryParams P(SCENE_QUERY_STAT(VoxelActorLOS), false);
	if (Ignore)
		P.AddIgnoredActor(Ignore);
	for (int32 I = 0; I < 64; ++I)
	{
		FHitResult H;
		if (!GetWorld()->LineTraceSingleByChannel(H, S, E, ECC_Visibility, P))
			return true;
		if (H.GetComponent() && H.GetComponent()->IsA<UVoxelCollisionComponent>())
		{
			P.AddIgnoredActor(H.GetActor());
			continue;
		}
		return false;
	}
	return false;
}
bool UVoxelModule::VerifyView(APlayerController* PC, AActor* Observer, const FVoxelEditIntent& I, FVoxelTraceResult& O, FString& E) const
{
	if (!IsReady() || !IsAuthority() || !Observer || Observer->GetWorld() != GetWorld() || I.Origin.ContainsNaN() || I.Direction.ContainsNaN() ||
	    I.Direction.IsNearlyZero())
		return false;
	FVector Eye;
	FRotator Rotation;
	Observer->GetActorEyesViewPoint(Eye, Rotation);
	if (PC && PC->GetPawn() == Observer)
		Rotation = PC->GetControlRotation();
	if (GetWorld()->GetNetMode() != NM_Standalone)
	{
		if (!PC || !PC->HasAuthority() || FVector::DistSquared(I.Origin, Eye) > 500.0 * 500.0 ||
		    FVector::DotProduct(Rotation.Vector(), I.Direction.GetSafeNormal()) < .984807753)
		{
			E = TEXT("View is outside the authorized observer");
			return false;
		}
		double Offset = FVector::Distance(Eye, I.Origin);
		if (Offset > 1)
		{
			auto C = Trace(Eye, I.Origin - Eye, Offset);
			if (C.Status == EVoxelTraceStatus::NeedsData || C.Status == EVoxelTraceStatus::Hit && C.Distance < Offset - 1 ||
			    !IsActorRayClear(Eye, I.Origin, Observer))
			{
				E = TEXT("Camera cannot see through an obstacle");
				return false;
			}
		}
	}
	O = Trace(I.Origin, I.Direction, 600);
	if (O.Status != EVoxelTraceStatus::Hit || O.Index != I.ExpectedTarget || FVector::Distance(O.Point, Eye) > 600 ||
	    !IsActorRayClear(I.Origin, O.Point - I.Direction.GetSafeNormal() * .1, Observer))
	{
		E = O.Status == EVoxelTraceStatus::NeedsData ? TEXT("Target data is not ready") : TEXT("Target is not visible or is too far");
		return false;
	}
	return true;
}
bool UVoxelModule::PlacementOverlapsActors(const FVoxelInteractionPlan& P) const
{
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_Pawn);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	for (const auto& E : P.Cells)
	{
		if (E.Value.IsAir())
			continue;
		const auto* D = Registry.GetSnapshot()->Find(E.Value.TypeId);
		if (!D || !D->bSolid)
			continue;
		for (const FBox& B : Shapes->Get(D->Shape, E.Value.State).CollisionBoxes)
		{
			FVector Center = (FVector(E.Position) + B.GetCenter()) * BlockSize(), Half = B.GetExtent() * BlockSize() - FVector(.1);
			if (Half.GetMin() <= 0)
				continue;
			if (GetWorld()->OverlapAnyTestByObjectType(Center, FQuat::Identity, Objects, FCollisionShape::MakeBox(Half)))
				return true;
		}
	}
	return false;
}
UAbilityInventoryBase* UVoxelModule::ResolveInventory(APlayerController* PC, AActor* Source) const
{
	AActor* A = PC && PC->GetPawn() ? PC->GetPawn() : Source;
	auto* I = Cast<IAbilityInventoryAgentInterface>(A);
	return I ? I->GetInventory() : nullptr;
}
FVoxelEditReply UVoxelModule::ExecuteIntent(APlayerController* PC, AActor* Source, const FVoxelEditIntent& I, bool Creative)
{
	FVoxelEditReply Reply;
	Reply.RequestId = I.RequestId;
	Reply.Code = EVoxelEditCode::Rejected;
	if (!I.RequestId || !Source || bMutating || !IsReady() || !IsAuthority())
	{
		Reply.Code = EVoxelEditCode::Busy;
		return Reply;
	}
	if (I.Action == EVoxelEditAction::BreakCancel)
	{
		Breaking.Remove(Source);
		Reply.Code = EVoxelEditCode::Accepted;
		return Reply;
	}
	FVoxelTraceResult H;
	if (!VerifyView(PC, Source, I, H, Reply.Reason))
	{
		Reply.Code = H.Status == EVoxelTraceStatus::NeedsData ? EVoxelEditCode::NeedsData : EVoxelEditCode::Rejected;
		Breaking.Remove(Source);
		return Reply;
	}
	double Now = FPlatformTime::Seconds();
	if (I.Action == EVoxelEditAction::BreakBegin)
	{
		FBreak B;
		B.Target = H.Index;
		B.Expected = H.State;
		B.Began = B.LastPulse = Now;
		Breaking.Add(Source, B);
		Reply.Code = EVoxelEditCode::Pending;
		return Reply;
	}
	if (I.Action == EVoxelEditAction::BreakPulse)
	{
		auto* B = Breaking.Find(Source);
		if (!B || B->Target != H.Index || B->Expected != H.State || Now - B->LastPulse > .35)
		{
			Breaking.Remove(Source);
			Reply.Code = EVoxelEditCode::Stale;
			return Reply;
		}
		B->LastPulse = Now;
		int32 Milliseconds = Registry.GetSnapshot()->Find(H.State.TypeId)->BreakMilliseconds;
		if ((Now - B->Began) * 1000 < Milliseconds)
		{
			Reply.Code = EVoxelEditCode::Pending;
			return Reply;
		}
	}
	if (I.Action == EVoxelEditAction::ContainerTake || I.Action == EVoxelEditAction::ContainerPut)
		return TransferContainer(PC, Source, I, H);
	UAbilityInventoryBase* Inventory = ResolveInventory(PC, Source);
	UAbilityInventorySlotBase* Slot = nullptr;
	uint16 PlaceType = 0;
	const auto* Agent = Source->FindComponentByClass<UVoxelAgentComponent>();
	Creative = Creative && GetWorld()->GetNetMode() == NM_Standalone && Agent && Agent->bCreativeInStandalone;
	FAbilityItem Before, After;
	if (I.Action == EVoxelEditAction::Place)
	{
		const auto* Def = Registry.GetSnapshot()->Find(I.ExpectedItemID);
		if (!Def)
		{
			Reply.Reason = TEXT("Unknown inventory block");
			return Reply;
		}
		PlaceType = Def->TypeId;
		if (!Creative)
		{
			Slot = Inventory ? Inventory->GetSlotBySplitTypeAndIndex(ESlotSplitType::Shortcut, I.InventorySlot) : nullptr;
			if (!Slot || Slot != Inventory->GetSelectedSlot(ESlotSplitType::Shortcut) || !Slot->IsEnabled() || Slot->GetItem().ID != I.ExpectedItemID ||
			    Slot->GetItem().Count <= 0 || Slot->GetItem().Level != 0)
			{
				Reply.Reason = TEXT("Server inventory slot does not contain this block");
				return Reply;
			}
			Before = Slot->GetItem();
			After = Before;
			--After.Count;
			if (After.Count == 0)
				After = FAbilityItem::Empty;
		}
	}
	FVoxelInteractionPlan Plan;
	if (!FVoxelEditTransaction::Build(*Runtime, *Registry.GetSnapshot(), *Shapes, H, I.Action, PlaceType, I.Direction, BlockSize(), Plan, Reply.Reason))
		return Reply;
	if ((I.Action == EVoxelEditAction::Place || I.Action == EVoxelEditAction::Use) && PlacementOverlapsActors(Plan))
	{
		Reply.Reason = TEXT("Result would overlap an actor");
		return Reply;
	}
	FVoxelPreparedEdit Prepared;
	if (!Runtime->PrepareEdit(Plan.Cells, Plan.Entities, Prepared, Reply.Reason))
		return Reply;
	TGuardValue<bool> Guard(bMutating, true);
	AAbilityPickUpVoxel* Drop = nullptr;
	if (Plan.DropID.IsValid() && Plan.DropCount > 0)
	{
		auto* C = GetColumn({VoxelCoord::Section(H.Index).X, VoxelCoord::Section(H.Index).Y}, true);
		Drop = AAbilityPickUpVoxel::CreateReserved(GetWorld(), FAbilityItem(Plan.DropID, Plan.DropCount), H.Point, C);
		if (!Drop)
		{
			Reply.Reason = TEXT("Cannot allocate a drop; block was not changed");
			return Reply;
		}
	}
	if (Slot && !FVoxelInventoryTransaction::SetSilent(*Slot, Before, After))
	{
		if (Drop)
			Drop->Destroy();
		Reply.Code = EVoxelEditCode::Stale;
		return Reply;
	}
	FVoxelEditBatch Batch;
	if (!Runtime->CommitEdit(MoveTemp(Prepared), Batch))
	{
		if (Slot)
			FVoxelInventoryTransaction::RestoreSilent(*Slot, Before);
		if (Drop)
			Drop->Destroy();
		Reply.Code = EVoxelEditCode::Stale;
		return Reply;
	}
	if (Drop)
		Drop->ActivateReserved();
	if (Slot)
		FVoxelInventoryTransaction::Notify(*Slot, Before);
	Breaking.Remove(Source);
	OnBlocksCommitted.Broadcast(Batch);
	Reply.Code = EVoxelEditCode::Accepted;
	return Reply;
}
FVoxelEditReply UVoxelModule::TransferContainer(APlayerController* PC, AActor* Source, const FVoxelEditIntent& I, const FVoxelTraceResult& H)
{
	FVoxelEditReply Reply;
	Reply.RequestId = I.RequestId;
	auto* S = Runtime->Find(VoxelCoord::Section(H.Index));
	if (!S || S->Stamp.Revision != I.ExpectedRevision || I.ContainerSlot < 0 || I.ContainerSlot >= 27)
	{
		Reply.Code = EVoxelEditCode::Stale;
		return Reply;
	}
	auto* Entity = S->Overlay.Entities.Find(VoxelCoord::Linear(VoxelCoord::Local(H.Index)));
	TArray<FVoxelItemStack> Items;
	auto* Inv = ResolveInventory(PC, Source);
	auto* Slot = Inv ? Inv->GetSlotBySplitTypeAndIndex(ESlotSplitType::Shortcut, I.InventorySlot) : nullptr;
	if (!Slot || !Slot->IsEnabled() || !Entity || !FVoxelBlockEntityCodec::DecodeContainer(*Entity, Items))
		return Reply;
	FAbilityItem Before = Slot->GetItem(), After = Before;
	auto& Cell = Items[I.ContainerSlot];
	int32 N = I.Count;
	if (N <= 0)
		return Reply;
	if (I.Action == EVoxelEditAction::ContainerTake)
	{
		if (Cell.Count < N)
			return Reply;
		FAbilityItem Take(Cell.ID, N, Cell.Level);
		if (!Slot->MatchItem(Take, true) || Slot->GetRemainVolume(Take) < N)
			return Reply;
		After = Before.Count > 0 ? FAbilityItem(Before.ID, Before.Count + N, Before.Level) : Take;
		Cell.Count -= N;
		if (Cell.Count == 0)
			Cell = {};
	}
	else
	{
		if (Before.Count < N || !Before.ID.IsValid() || Before.ID != I.ExpectedItemID || Cell.Count > 0 && (Cell.ID != Before.ID || Cell.Level != Before.Level))
			return Reply;
		auto* Data = UAssetModuleStatics::LoadPrimaryAsset<UAbilityItemDataBase>(Before.ID, false);
		if (!Data || N > Data->MaxCount - Cell.Count)
			return Reply;
		Cell.ID = Before.ID;
		Cell.Level = Before.Level;
		Cell.Count += N;
		After.Count -= N;
		if (After.Count == 0)
			After = FAbilityItem::Empty;
	}
	FVoxelEntityEdit E;
	E.Position = H.Index;
	if (!FVoxelBlockEntityCodec::EncodeContainer(Items, E.Value))
		return Reply;
	FVoxelPreparedEdit P;
	TArray<FVoxelEntityEdit> Writes{E};
	if (!Runtime->PrepareEdit({}, Writes, P, Reply.Reason))
		return Reply;
	TGuardValue<bool> Guard(bMutating, true);
	if (!FVoxelInventoryTransaction::SetSilent(*Slot, Before, After))
	{
		Reply.Code = EVoxelEditCode::Stale;
		return Reply;
	}
	FVoxelEditBatch Batch;
	if (!Runtime->CommitEdit(MoveTemp(P), Batch))
	{
		FVoxelInventoryTransaction::RestoreSilent(*Slot, Before);
		Reply.Code = EVoxelEditCode::Stale;
		return Reply;
	}
	FVoxelInventoryTransaction::Notify(*Slot, Before);
	OnBlocksCommitted.Broadcast(Batch);
	Reply.Code = EVoxelEditCode::Accepted;
	return Reply;
}
bool UVoxelModule::ApplyPrefab(const FVoxelPrefabSaveData& P, const FIntVector& Origin, FString& E)
{
	if (!IsAuthority() || !IsReady() || bMutating || P.Cells.IsEmpty() || P.Cells.Num() > VoxelPrefab::MaxCellCount)
	{
		E = TEXT("Invalid prefab operation");
		return false;
	}
	if (!UVoxelPrefabData::ValidateCells(P, *Registry.GetSnapshot(), E))
		return false;
	TArray<FVoxelCellEdit> Cells;
	TSet<FIntVector> Seen;
	for (const auto& C : P.Cells)
	{
		int64 X = int64(Origin.X) + C.Offset.X, Y = int64(Origin.Y) + C.Offset.Y, Z = int64(Origin.Z) + C.Offset.Z;
		if (FMath::Abs(X) >= VoxelBlock::MaxAbsCoordinate || FMath::Abs(Y) >= VoxelBlock::MaxAbsCoordinate || FMath::Abs(Z) >= VoxelBlock::MaxAbsCoordinate)
			return false;
		FVoxelCellEdit W;
		W.Position = FIntVector(int32(X), int32(Y), int32(Z));
		if (Seen.Contains(W.Position) || !Runtime->TryGetBlock(W.Position, W.Expected) || !FVoxelItemBridge::ToBlock(*Registry.GetSnapshot(), C.Item, W.Value))
			return false;
		Seen.Add(W.Position);
		Cells.Add(W);
	}
	if (!FVoxelEditTransaction::ValidateBatch(*Runtime, *Registry.GetSnapshot(), *Shapes, Cells, E))
		return false;
	FVoxelInteractionPlan Plan;
	Plan.Cells = Cells;
	if (PlacementOverlapsActors(Plan))
	{
		E = TEXT("Prefab overlaps live actors");
		return false;
	}
	if (Cells.IsEmpty())
	{
		E = TEXT("Prefab makes no change");
		return false;
	}
	FVoxelPreparedEdit Prepared;
	if (!Runtime->PrepareEdit(Cells, {}, Prepared, E))
		return false;
	TGuardValue<bool> Guard(bMutating, true);
	FVoxelEditBatch Batch;
	if (!Runtime->CommitEdit(MoveTemp(Prepared), Batch))
		return false;
	OnBlocksCommitted.Broadcast(Batch);
	return true;
}
bool UVoxelModule::ExportPrefab(const FIntVector& Min, const FIntVector& Max, FVoxelPrefabSaveData& O, FString& E) const
{
	if (!IsReady() || !VoxelCoord::IsValid(Min) || !VoxelCoord::IsValid(Max))
		return false;
	int64 SX = int64(Max.X) - Min.X, SY = int64(Max.Y) - Min.Y, SZ = int64(Max.Z) - Min.Z;
	if (SX <= 0 || SY <= 0 || SZ <= 0 || SX > VoxelPrefab::MaxCellCount || SY > VoxelPrefab::MaxCellCount ||
	    SZ > VoxelPrefab::MaxCellCount || SX * SY * SZ > VoxelPrefab::MaxCellCount)
	{
		E = FString::Printf(TEXT("Prefab bounds exceed %d cells"), VoxelPrefab::MaxCellCount);
		return false;
	}
	FVoxelPrefabSaveData P;
	for (int32 Z = Min.Z; Z < Max.Z; ++Z)
		for (int32 Y = Min.Y; Y < Max.Y; ++Y)
			for (int32 X = Min.X; X < Max.X; ++X)
			{
				FIntVector Position(X, Y, Z);
				FVoxelBlockState B;
				if (!Runtime->TryGetBlock(Position, B))
				{
					E = TEXT("Prefab selection is not fully loaded");
					return false;
				}
				if (B.IsAir())
					continue;
				const auto* Def = Registry.GetSnapshot()->Find(B.TypeId);
				if (Def && Def->EntityKind)
				{
					const auto* S = Runtime->Find(VoxelCoord::Section(Position));
					const auto* Entity = S->Overlay.Entities.Find(VoxelCoord::Linear(VoxelCoord::Local(Position)));
					FVoxelBlockEntityState Default;
					if (!Entity || !FVoxelBlockEntityCodec::MakeDefault(Def->EntityKind, Default, Def->EntityVariant) || !(*Entity == Default))
					{
						E = TEXT("Prefab export supports definitions, not live entity contents; reset entity state explicitly first");
						return false;
					}
				}
				FVoxelPrefabCell C;
				C.Offset = Position - Min;
				if (!FVoxelItemBridge::ToItem(*Registry.GetSnapshot(), B, 1, C.Item))
					return false;
				P.Cells.Add(C);
			}
	if (!UVoxelPrefabData::ValidateCells(P, *Registry.GetSnapshot(), E))
	{
		return false;
	}
	O = MoveTemp(P);
	return true;
}
bool UVoxelModule::CaptureColumnForUnload(UVoxelChunk& C, FString& E)
{
	TArray<uint8> B;
	if (!C.CaptureActors(B, E))
		return false;
	uint64 Bytes = B.Num();
	FString Path = FVoxelSceneColumnCodec::RelativePath(C.GetColumn());
	for (const auto& P : UnloadedSceneFiles)
		if (P.Key != Path)
			Bytes += P.Value.Num();
	if (Bytes > 32 * 1024 * 1024)
	{
		E = TEXT("Save scene changes before unloading more columns");
		return false;
	}
	UnloadedSceneFiles.Add(Path, MoveTemp(B));
	return true;
}
void UVoxelModule::QueueSceneLoad(UVoxelChunk& C, const FVoxelSection& S)
{
	FString Relative = FVoxelSceneColumnCodec::RelativePath(C.GetColumn());
	if (auto* Bytes = UnloadedSceneFiles.Find(Relative))
	{
		FString E;
		C.bSceneLoading = true;
		if (!C.RestoreActors(*Bytes, E))
		{
			C.bSceneLoading = false;
			C.bSceneFailed = true;
		}
		return;
	}
	FVoxelTaskRequest Q;
	Q.Stamp = S.Stamp;
	Q.Kind = EVoxelTaskKind::LoadScene;
	Q.Priority = -200000;
	Q.ReservedBytes = 8 * 1024 * 1024;
	FString Root = RegionStore.GetSourceDirectory(), Path = FPaths::Combine(Root, Relative);
	auto Stamp = S.Stamp;
	Q.Execute = [Root, Path, Stamp](const std::atomic_bool& Cancel)
	{
		FVoxelTaskResult O;
		O.Stamp = Stamp;
		O.Kind = EVoxelTaskKind::LoadScene;
		int64 Size = Root.IsEmpty() ? -1 : IFileManager::Get().FileSize(*Path);
		if (Size < 0)
		{
			O.bSuccess = true;
			return O;
		}
		if (Size > 8 * 1024 * 1024)
		{
			O.Error = TEXT("Scene file exceeds limit");
			return O;
		}
		O.bSuccess = FFileHelper::LoadFileToArray(O.Bytes, *Path) && !Cancel.load();
		O.bCanceled = Cancel.load();
		return O;
	};
	if (Scheduler->Enqueue(MoveTemp(Q)))
		C.bSceneLoading = true;
}
TUniquePtr<FVoxelWorldSaveData> UVoxelModule::NewWorldData(const FParameter& P) const
{
	auto O = MakeUnique<FVoxelModuleSaveData>(WorldBasicData);
	if (P.HasValue() && P.GetStructType() && P.GetStructType()->IsChildOf(FVoxelWorldSaveData::StaticStruct()))
		static_cast<FVoxelWorldSaveData&>(*O) = *reinterpret_cast<const FVoxelWorldSaveData*>(P.GetStructMemory());
	return O;
}
bool UVoxelModule::ValidateWorldData(const FParameter& P, FString& E) const
{
	if (!P.HasValue() || !P.GetStructType() || !P.GetStructType()->IsChildOf(FVoxelWorldSaveData::StaticStruct()) || !P.GetStructMemory())
	{
		E = TEXT("Expected typed voxel world save data");
		return false;
	}
	const auto& D = *reinterpret_cast<const FVoxelWorldSaveData*>(P.GetStructMemory());
	FVoxelWorldManifest M;
	if (!FVoxelManifestCodec::Decode(D.ManifestBytes, M) || !Registry.GetSnapshot() ||
		!WorldGenerationProfile || M.RegistryHash != Registry.GetSnapshot()->Hash ||
		M.CatalogHash != WorldGenerationProfile->CatalogHash)
	{
		E = TEXT("Save generator/catalog/registry mismatch; original save is left unchanged");
		return false;
	}
	FVoxelGenerationRuntimeConfig C;
	if (!FVoxelGenerationBinding::Build(*WorldGenerationProfile, *Registry.GetSnapshot(),
		M.Settings, M.BlockSizeCentimeters, C, E))
		return false;
	FVoxelGenerationPipeline G(C);
	uint64 H = 0;
	if (!G.BuildHandshakeSignature(H) || H != M.BaseSampleHash)
	{
		E = TEXT("Saved base generation signature differs");
		return false;
	}
	return true;
}
void UVoxelModule::LoadData(const FParameter& P, EPhase Phase)
{
	if (!IsAuthority())
		return;
	if (PHASEC(Phase, EPhase::Primary))
	{
		FString E;
		const bool Typed = P.HasValue() && P.GetStructType() && P.GetStructMemory() &&
			P.GetStructType()->IsChildOf(FVoxelWorldSaveData::StaticStruct());
		const auto* D = Typed ? reinterpret_cast<const FVoxelWorldSaveData*>(P.GetStructMemory()) : nullptr;
		const bool NewRequest = D && D->ManifestBytes.IsEmpty() && RegionStore.GetSourceDirectory().IsEmpty();
		if (NewRequest)
		{
			if (Runtime)
			{
				E = TEXT("Close the current preview/world before creating a new world");
				UE_LOG(LogTemp, Error, TEXT("%s"), *E);
				return;
			}
			WorldData = NewWorldData(P);
			if (!CreateWorldFromProfile(D->Generation.Seed, E))
			{
				WorldState = EVoxelWorldState::Failed;
				bWorldLoadRejected = true;
				UE_LOG(LogTemp, Error, TEXT("Voxel new world: %s"), *E);
				return;
			}
		}
		else
		{
			if (!ValidateWorldData(P, E))
			{
				bWorldLoadRejected = true;
				WorldState = EVoxelWorldState::Failed;
				LastSaveError = E;
				UE_LOG(LogTemp, Error, TEXT("Voxel load rejected, no fallback/save rewrite: %s"), *E);
				return;
			}
			FVoxelWorldManifest M;
			if (!FVoxelManifestCodec::Decode(D->ManifestBytes, M) || !StopWorld(true, E))
				return;
			WorldData = NewWorldData(P);
			if (!StartWorld(M, false, E))
			{
				bWorldLoadRejected = true;
				WorldState = EVoxelWorldState::Failed;
				UE_LOG(LogTemp, Error, TEXT("Voxel start: %s"), *E);
				return;
			}
		}
	}
	if (PHASEC(Phase, EPhase::Final) && IsReady())
	{
		USceneModule& SceneModule = USceneModule::Get();
		SceneModule.LoadSaveData(FParameter(WorldData->SceneData), Phase);
		SceneModule.SetSeaLevel(Manifest.Settings.SeaLevel * BlockSize());
	}
}
FParameter UVoxelModule::ToData()
{
	if (!IsReady() || !LastSaveError.IsEmpty())
		return FParameter();
	FVoxelModuleSaveData D(WorldBasicData);
	static_cast<FVoxelWorldSaveData&>(D) = *WorldData;
	FVoxelManifestCodec::Encode(Manifest, D.ManifestBytes);
	D.Generation = Manifest.Settings;
	D.BlockSizeCentimeters = Manifest.BlockSizeCentimeters;
	const auto SceneSave = USceneModule::Get().GetSaveData(true);
	const auto* SD = SceneSave.GetPtr<FSceneModuleSaveData>();
	if (!SD)
		return FParameter();
	D.SceneData = *SD;
	return FParameter(MoveTemp(D));
}
FParameter UVoxelModule::GetData()
{
	return ToData();
}
void UVoxelModule::UnloadData(EPhase P)
{
	if (PHASEC(P, EPhase::Primary))
	{
		FString E;
		if (!StopWorld(true, E))
			UE_LOG(LogTemp, Error, TEXT("Voxel unload: %s"), *E);
	}
}
void UVoxelModule::SetActiveSaveSource(const FGuid& ID, int32 G, FSaveGameStorage* S)
{
	RegionStore.SetSource(ID, G, S);
}
void UVoxelModule::OnBeforeSaveData()
{
	LastSaveError.Reset();
	CapturedSceneFiles.Reset();
	if (!IsSaveEnabled())
		return;
	if (WorldView)
		WorldView->OnSaveBoundary(true);
	if (!Runtime)
	{
		LastSaveError = TEXT("Create or load the world before capturing it");
		if (WorldView)
			WorldView->OnSaveBoundary(false);
		return;
	}
	if (bMutating || !SaveAdapter.Capture(*Runtime, Manifest, Registry.GetSnapshot(), RegionStore, LastSaveError))
	{
		if (LastSaveError.IsEmpty())
			LastSaveError = TEXT("Voxel save capture is busy");
		if (WorldView)
			WorldView->OnSaveBoundary(false);
		return;
	}
	CapturedSceneFiles = UnloadedSceneFiles;
	for (const auto& P : Columns)
		if (P.Value && P.Value->bSceneReady)
		{
			TArray<uint8> B;
			if (!P.Value->CaptureActors(B, LastSaveError))
				break;
			CapturedSceneFiles.Add(FVoxelSceneColumnCodec::RelativePath(P.Key), MoveTemp(B));
		}
}
bool UVoxelModule::CopySaveCapture(FVoxelModuleSaveCapture& O, FString& E) const
{
	if (!LastSaveError.IsEmpty() || !SaveAdapter.GetCapture())
	{
		E = LastSaveError.IsEmpty() ? TEXT("Voxel save was not captured") : LastSaveError;
		return false;
	}
	O.Voxels = *SaveAdapter.GetCapture();
	O.SceneFiles = CapturedSceneFiles;
	return true;
}
bool UVoxelModule::WriteSaveCapture(const FVoxelModuleSaveCapture& C, const FString& Dir, FString& E)
{
	if (!FVoxelWorldSaveAdapter::WriteCapture(C.Voxels, Dir, E))
		return false;
	for (const auto& P : C.SceneFiles)
	{
		if (!P.Key.StartsWith(TEXT("voxel/actors/c_")) || P.Key.Contains(TEXT("..")) || P.Value.Num() > 8 * 1024 * 1024)
			return false;
		FString Path = FPaths::Combine(Dir, P.Key);
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
		if (!FFileHelper::SaveArrayToFile(P.Value, *Path))
		{
			E = TEXT("Scene actor file write failed");
			return false;
		}
	}
	return true;
}
void UVoxelModule::OnAfterSaveData(bool Success)
{
	if (Runtime && SaveAdapter.IsBusy())
		SaveAdapter.Complete(*Runtime, RegionStore, Success, PendingCommitDirectory);
	if (Success)
		for (const auto& P : CapturedSceneFiles)
			if (const auto* B = UnloadedSceneFiles.Find(P.Key))
				if (*B == P.Value)
					UnloadedSceneFiles.Remove(P.Key);
	CapturedSceneFiles.Reset();
	LastSaveError.Reset();
	PendingCommitDirectory.Reset();
	if (WorldView)
		WorldView->OnSaveBoundary(false);
}

bool UVoxelModule::QueueRemoteEncoded(const TArray<uint8>& P, FString& E)
{
	if (!IsReady() || IsAuthority() || P.IsEmpty() || P.Num() > 2 * 1024 * 1024)
		return false;
	uint64 Bytes = P.Num();
	for (const auto& B : EncodedRemotePending)
		Bytes += B.Num();
	if (EncodedRemotePending.Num() + RemotePending.Num() + int32(bDecodeRunning) + int32(bRemoteRunning) >= 8 || Bytes > 8 * 1024 * 1024)
	{
		E = TEXT("Remote decode backlog exceeds the bounded budget");
		return false;
	}
	EncodedRemotePending.Add(P);
	return true;
}
void UVoxelModule::PumpDecode()
{
	if (IsAuthority() || bDecodeRunning || EncodedRemotePending.IsEmpty() || RemotePending.Num() >= 4)
		return;
	if (NextNetworkJob >= uint64(MAX_int32))
		return;
	uint64 ID = NextNetworkJob++;
	FVoxelTaskRequest Q;
	Q.Kind = EVoxelTaskKind::DecodeNetwork;
	Q.Stamp.WorldEpoch = Epoch;
	Q.Stamp.GenerationToken = ID;
	Q.Stamp.Key = {int32(ID), MAX_int32, MAX_int32};
	Q.Priority = -300000;
	Q.ReservedBytes = 32 * 1024 * 1024;
	Q.InputBytes = EncodedRemotePending[0].Num();
	auto Payload = EncodedRemotePending[0];
	auto RegistrySnapshot = Registry.GetSnapshot();
	auto WorldManifest = Manifest;
	Q.Execute = [Payload = MoveTemp(Payload), RegistrySnapshot, WorldManifest](const std::atomic_bool& Cancel)
	{
		FVoxelTaskResult O;
		FVoxelSnapshotBatch B;
		if (Cancel.load())
			return O;
		if (!FVoxelNetworkCodec::DecodeSnapshots(Payload, WorldManifest, *RegistrySnapshot, B))
			return O;
		O.BatchId = B.Id;
		O.RemoteOverlays = MoveTemp(B.Sections);
		O.bSuccess = true;
		return O;
	};
	if (Scheduler->Enqueue(MoveTemp(Q)))
	{
		EncodedRemotePending.RemoveAt(0);
		bDecodeRunning = true;
	}
}
bool UVoxelModule::QueueNetworkEncode(const FVoxelSnapshotBatch& B, UVoxelModuleNetworkComponent* Recipient)
{
	if (!IsReady() || !IsAuthority() || !Recipient || NextNetworkJob >= uint64(MAX_int32))
		return false;
	uint64 ID = NextNetworkJob++;
	FVoxelTaskRequest Q;
	Q.Kind = EVoxelTaskKind::EncodeNetwork;
	Q.Stamp.WorldEpoch = Epoch;
	Q.Stamp.GenerationToken = ID;
	Q.Stamp.Key = {int32(ID), MAX_int32, MAX_int32};
	Q.Priority = -200000;
	Q.ReservedBytes = 4 * 1024 * 1024;
	for (const auto& S : B.Sections)
	{
		Q.InputBytes += S.Blocks.Num() * 16;
		for (const auto& E : S.Entities)
			Q.InputBytes += E.Value.Payload.Num() + 64;
	}
	auto Snapshot = Registry.GetSnapshot();
	auto World = Manifest;
	auto Session = SessionId;
	Q.Execute = [B, Snapshot, World, Session](const std::atomic_bool& Cancel)
	{
		FVoxelTaskResult O;
		O.BatchId = B.Id;
		if (Cancel.load())
			return O;
		TArray<uint8> Payload;
		if (!FVoxelNetworkCodec::EncodeSnapshots(B, World, *Snapshot, Payload))
			return O;
		O.bSuccess = FVoxelNetworkCodec::Encode(EVoxelMessage::Snapshots, Session, Payload, O.Bytes);
		return O;
	};
	if (!Scheduler->Enqueue(MoveTemp(Q)))
		return false;
	NetworkRecipients.Add(ID, Recipient);
	NetworkBatchIds.Add(ID, B.Id);
	return true;
}

float UVoxelModule::GetWarmupProgress() const
{
	if (!Runtime)
		return 0;
	int32 Wanted = 0;
	int32 Ready = 0;
	for (const auto& D : Desired)
		if (D.Value.bCollision)
		{
			++Wanted;
			const auto* S = Runtime->Find(D.Key);
			if (S && S->Status == EVoxelSectionStatus::DataReady && S->bHasCollision && !S->bCollisionDirty)
				++Ready;
		}
	if (Wanted == 0)
	{
		return IsReady() ? 1.f : 0.f;
	}
	return float(Ready) / Wanted;
}

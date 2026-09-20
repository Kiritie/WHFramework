#include "Ability/PickUp/AbilityPickUpVoxel.h"
#include "Ability/PickUp/AbilityPickerInterface.h"
#include "Ability/Inventory/AbilityInventoryAgentInterface.h"
#include "Ability/Inventory/AbilityInventoryBase.h"
#include "Asset/AssetModuleStatics.h"
#include "Voxel/Voxels/Data/VoxelData.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/Scene/VoxelSceneRegion.h"
#include "Voxel/Components/VoxelMeshComponent.h"
#include "Voxel/Rendering/VoxelMaterialSet.h"
#include "Voxel/Network/VoxelModuleNetworkComponent.h"
#include "Scene/Actor/SceneActorInterface.h"
#include "Common/Interaction/InteractionComponent.h"
#include "Common/Movement/FallingMovementComponent.h"
#include "Common/Movement/FollowingMovementComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Misc/ScopeExit.h"
AAbilityPickUpVoxel::AAbilityPickUpVoxel()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);
	SetActorEnableCollision(false);
	if (BoxComponent)
		BoxComponent->SetGenerateOverlapEvents(false);
}
AAbilityPickUpVoxel* AAbilityPickUpVoxel::CreateReserved(UWorld* W, const FAbilityItem& I, const FVector& P, UVoxelSceneRegion* C)
{
	if (!W || W->GetNetMode() == NM_Client || !C || !I.ID.IsValid() || I.Count <= 0)
		return nullptr;
	const UVoxelData* D = UAssetModuleStatics::LoadPrimaryAsset<UVoxelData>(I.ID, false);
	if (!D || !D->PickUpClass || !D->PickUpClass->IsChildOf(StaticClass()))
		return nullptr;
	auto* A = W->SpawnActorDeferred<AAbilityPickUpVoxel>(D->PickUpClass, FTransform(P), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!A)
		return nullptr;
	A->RepID = I.ID;
	A->RepCount = I.Count;
	A->RepLevel = I.Level;
	A->Item = I;
	A->Item.Payload = A;
	A->OwningRegion = C;
	A->bActivated = false;
	ISceneActorInterface::Execute_SetActorID(A, FGuid::NewGuid().ToString());
	TScriptInterface<ISceneContainerInterface> Container;
	Container.SetObject(C);
	Container.SetInterface(C);
	ISceneActorInterface::Execute_SetContainer(A, Container);
	A->FinishSpawning(FTransform(P));
	A->SetActorEnableCollision(false);
	if (!C->AddSceneActor(A))
	{
		A->Destroy();
		return nullptr;
	}
	return A;
}
void AAbilityPickUpVoxel::ActivateReserved()
{
	if (!HasAuthority())
		return;
	bActivated = true;
	OnRepState();
	ForceNetUpdate();
}
void AAbilityPickUpVoxel::OnRepState()
{
	Item = FAbilityItem(RepID, RepCount, RepLevel);
	Item.Payload = this;
	bVisualDirty = true;
	SetActorHiddenInGame(!bActivated || RepCount <= 0);
	SetActorEnableCollision(HasAuthority() && bActivated && RepCount > 0);
	if (BoxComponent)
		BoxComponent->SetGenerateOverlapEvents(HasAuthority() && bActivated && RepCount > 0);
	if (Interaction)
		Interaction->SetInteractable(HasAuthority() && bActivated && RepCount > 0);
	if (FallingMovement)
		FallingMovement->SetActive(HasAuthority() && bActivated);
	if (FollowingMovement)
		FollowingMovement->SetActive(HasAuthority() && bActivated);
}
void AAbilityPickUpVoxel::LoadData(const FParameter& D, EPhase)
{
	if (!HasAuthority())
		return;
	const FPickUpSaveData* P = D.GetPtr<FPickUpSaveData>();
	if (!P || !P->Item.ID.IsValid() || P->Item.Count <= 0)
		return;
	RepID = P->Item.ID;
	RepCount = P->Item.Count;
	RepLevel = P->Item.Level;
	SetActorLocation(P->Location);
	bActivated = true;
	OnRepState();
	ForceNetUpdate();
}
FParameter AAbilityPickUpVoxel::ToData()
{
	FPickUpSaveData D;
	D.Item = FAbilityItem(RepID, RepCount, RepLevel);
	D.Location = GetActorLocation();
	return FParameter(MoveTemp(D));
}
void AAbilityPickUpVoxel::Tick(float Dt)
{
	Super::Tick(Dt);
	if (bVisualDirty && bActivated && GetNetMode() != NM_DedicatedServer)
		bVisualDirty = !BuildVisual();
}
bool AAbilityPickUpVoxel::BuildVisual()
{
	auto* M = UVoxelModule::GetPtr();
	if (!M || !M->IsReady() || !M->GetMaterialSet())
		return false;
	const auto* D = M->GetRegistry()->Find(RepID);
	if (!D)
		return false;
	FVoxelSectionSnapshot S;
	S.Blocks.Init(0, 4096);
	S.Blocks[0] = FVoxelBlockState{D->TypeId, 0}.Pack();
	FVoxelSectionMeshResult Result;
	if (!FVoxelSectionMesher::Build(S, *M->GetRegistry(), *M->GetShapes(), Result))
		return false;
	for (auto& C : DisplayMeshes)
		if (C)
			C->DestroyComponent();
	DisplayMeshes.Reset();
	for (const auto& B : Result.Batches)
	{
		auto* Bank = M->GetMaterialSet()->FindBank(B.Group, B.Bank);
		if (!Bank || !Bank->Material)
			return false;
		auto* C = NewObject<UVoxelMeshComponent>(this);
		C->SetupAttachment(GetRootComponent());
		C->SetRelativeLocation(FVector(-.15 * M->BlockSize()));
		C->RegisterComponent();
		if (!C->Apply(B.Mesh, M->BlockSize() * .3, Bank->Material))
		{
			C->DestroyComponent();
			return false;
		}
		DisplayMeshes.Add(C);
	}
	return true;
}
UMeshComponent* AAbilityPickUpVoxel::GetMeshComponent() const
{
	return DisplayMeshes.IsEmpty() ? nullptr : DisplayMeshes[0].Get();
}
void AAbilityPickUpVoxel::OnPickUp(IAbilityPickerInterface* Picker)
{
	if (!HasAuthority() || !bActivated || RepCount <= 0 || !Picker)
		return;
	auto* M = UVoxelModule::GetPtr();
	if (!M || !M->BeginProjectMutation())
		return;
	ON_SCOPE_EXIT
	{
		M->EndProjectMutation();
	};
	AActor* A = Cast<AActor>(Picker);
	auto* Agent = Cast<IAbilityInventoryAgentInterface>(A);
	auto* Inventory = Agent ? Agent->GetInventory() : nullptr;
	if (!Inventory)
		return;
	FAbilityItem Remaining(RepID, RepCount, RepLevel);
	Inventory->AddItemByRange(Remaining);
	if (Remaining.Count < 0 || Remaining.Count > RepCount)
		return;
	RepCount = Remaining.Count;
	Item.Count = RepCount;
	ForceNetUpdate();
	UVoxelModuleNetworkComponent::PushInventoryFor(A);
	if (RepCount == 0)
	{
		bActivated = false;
		if (OwningRegion)
			OwningRegion->RemoveSceneActor(this);
		SetActorEnableCollision(false);
		Destroy();
	}
}
void AAbilityPickUpVoxel::OnBeginOverlap(UPrimitiveComponent*, AActor* A, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	if (HasAuthority() && bActivated)
		if (auto* P = Cast<IAbilityPickerInterface>(A))
			if (P->IsAutoPickUp())
				OnPickUp(P);
}
void AAbilityPickUpVoxel::OnEnterInteract(IInteractionAgentInterface* A)
{
	if (HasAuthority() && bActivated)
		Super::OnEnterInteract(A);
}
void AAbilityPickUpVoxel::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAbilityPickUpVoxel, RepID);
	DOREPLIFETIME(AAbilityPickUpVoxel, RepCount);
	DOREPLIFETIME(AAbilityPickUpVoxel, RepLevel);
	DOREPLIFETIME(AAbilityPickUpVoxel, bActivated);
}

void AAbilityPickUpVoxel::SetContainer_Implementation(const TScriptInterface<ISceneContainerInterface>& C)
{
	if (OwningRegion)
		OwningRegion->RemoveSceneActor(this);
	Super::SetContainer_Implementation(C);
	OwningRegion = Cast<UVoxelSceneRegion>(C.GetObject());
	if (OwningRegion)
		OwningRegion->AddSceneActor(this);
}

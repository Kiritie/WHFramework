#include "Ability/Inventory/InventoryPickupActor.h"

#include "Ability/Inventory/InventoryItemAssetCatalog.h"
#include "Ability/Inventory/InventoryWorldSubsystem.h"
#include "Components/BillboardComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/Texture2D.h"
#include "Net/UnrealNetwork.h"
#include "Ability/Interaction/AbilityInteractionOptions.h"
#include "Common/Interaction/InteractionComponent.h"

AInventoryPickupActor::AInventoryPickupActor()
{
    bReplicates = true;
    SetReplicateMovement(true);
    PrimaryActorTick.bCanEverTick = false;
    Collision = CreateDefaultSubobject<USphereComponent>(TEXT("PickupCollision"));
    Collision->SetSphereRadius(24.f);
    Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
    Collision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    Collision->SetGenerateOverlapEvents(true);
    SetRootComponent(Collision);
    Interaction = CreateDefaultSubobject<UInteractionComponent>(TEXT("Interaction"));
    Interaction->SetupAttachment(Collision);
    Interaction->SetGenerateOverlapEvents(false);
    Interaction->SetBoxExtent(FVector(120.f));
    Interaction->Options.Add(CreateDefaultSubobject<UInteractionOption_AbilityPickUp>(TEXT("PickupOption")));
    Presentation = CreateDefaultSubobject<UBillboardComponent>(TEXT("PickupPresentation"));
    Presentation->SetupAttachment(Collision);
    Presentation->SetHiddenInGame(false);
    Presentation->SetRelativeScale3D(FVector(0.2f));
}

void AInventoryPickupActor::EndPlay(const EEndPlayReason::Type InReason)
{
    if (System.IsValid())
    {
        System->OnCommitted.Remove(CommitHandle);
        System->OnDeleted.Remove(DeleteHandle);
    }
    System.Reset();
    Super::EndPlay(InReason);
}

void AInventoryPickupActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AInventoryPickupActor, InventoryId);
    DOREPLIFETIME(AInventoryPickupActor, PickupAction);
    DOREPLIFETIME(AInventoryPickupActor, PublicItemId);
    DOREPLIFETIME(AInventoryPickupActor, PublicCount);
}

bool AInventoryPickupActor::BindInventory(UInventoryWorldSubsystem& InSystem, const FGuid& InInventoryId)
{
    if (!HasAuthority() || InSystem.GetWorld() != GetWorld() || !InInventoryId.IsValid())
    {
        return false;
    }
    if (System.IsValid())
    {
        System->OnCommitted.Remove(CommitHandle);
        System->OnDeleted.Remove(DeleteHandle);
    }
    System = &InSystem;
    InventoryId = InInventoryId;
    if (!RefreshFromSnapshot())
    {
        System.Reset();
        return false;
    }
    CommitHandle = InSystem.OnCommitted.AddUObject(this, &AInventoryPickupActor::HandleCommit);
    DeleteHandle = InSystem.OnDeleted.AddUObject(this, &AInventoryPickupActor::HandleDeleted);
    return true;
}

void AInventoryPickupActor::InitializePresentation(const FGuid& InInventoryId, const FPrimaryAssetId& InItemId, int32 InCount)
{
    if (!HasAuthority())
    {
        return;
    }
    InventoryId = InInventoryId;
    PublicItemId = InItemId;
    PublicCount = InCount;
    OnRep_Presentation();
    ForceNetUpdate();
}

FGuid AInventoryPickupActor::GetInventoryId() const
{
    return InventoryId;
}

int32 AInventoryPickupActor::GetPublicCount() const
{
    return PublicCount;
}

FPrimaryAssetId AInventoryPickupActor::GetPublicItemId() const
{
    return PublicItemId;
}

void AInventoryPickupActor::HandleCommit(const FInventoryChangeSet& InChanges)
{
    if (InChanges.Revisions.ContainsByPredicate([this](const FInventoryRevision& Revision) { return Revision.InventoryId == InventoryId; }))
    {
        RefreshFromSnapshot();
    }
}

void AInventoryPickupActor::HandleDeleted(const FGuid& InInventoryId)
{
    if (InInventoryId == InventoryId)
    {
        PublicCount = 0;
        OnRep_Presentation();
        ForceNetUpdate();
    }
}

bool AInventoryPickupActor::RefreshFromSnapshot()
{
    FInventorySnapshot Snapshot;
    FString Error;
    if (!System.IsValid() || !System->GetSnapshot(InventoryId, Snapshot, Error) || Snapshot.Slots.Num() != 1)
    {
        return false;
    }
    InitializePresentation(InventoryId, Snapshot.Slots[0].Item.ItemId, Snapshot.Slots[0].Item.Count);
    return true;
}

void AInventoryPickupActor::OnRep_Presentation()
{
    UInventoryWorldSubsystem* WorldSystem = GetWorld() ? GetWorld()->GetSubsystem<UInventoryWorldSubsystem>() : nullptr;
    const UInventoryItemAssetCatalog* Assets = WorldSystem ? WorldSystem->GetItemAssets() : nullptr;
    UTexture2D* Texture = Assets ? Cast<UTexture2D>(Assets->FindIcon(PublicItemId)) : nullptr;
    Presentation->SetSprite(Texture);
    SetActorHiddenInGame(PublicCount <= 0);
    SetActorEnableCollision(PublicCount > 0);
    if(Interaction)
    {
        Interaction->SetInteractable(PublicCount > 0 && !PickupAction.IsNone());
        Interaction->NotifyOptionsChanged();
    }
}

UInteractionComponent* AInventoryPickupActor::GetInteractionComponent() const
{
    return Interaction;
}

EInteractAgentType AInventoryPickupActor::GetInteractAgentType() const
{
    return EInteractAgentType::Passivity;
}

bool AInventoryPickupActor::IsInteractable(IInteractionAgentInterface* InAgent)
{
    return PublicCount > 0 && !PickupAction.IsNone() && IInteractionAgentInterface::IsInteractable(InAgent);
}

void AInventoryPickupActor::OnEnterInteract(IInteractionAgentInterface* InAgent)
{
    Presentation->SetRenderCustomDepth(true);
}

void AInventoryPickupActor::OnLeaveInteract(IInteractionAgentInterface* InAgent)
{
    Presentation->SetRenderCustomDepth(false);
}

void AInventoryPickupActor::SetPickupAction(FName InAction)
{
    if(!HasAuthority()) return;
    PickupAction = InAction;
    OnRep_Presentation();
    ForceNetUpdate();
}

FName AInventoryPickupActor::GetPickupAction() const
{
    return PickupAction;
}

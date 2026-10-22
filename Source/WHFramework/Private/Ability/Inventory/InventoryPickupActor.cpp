#include "Ability/Inventory/InventoryPickupActor.h"

#include "Ability/Inventory/InventoryItemAssetCatalog.h"
#include "Ability/Inventory/InventoryWorldSubsystem.h"
#include "Ability/Inventory/Widget/WidgetInventoryPickupIcon.h"
#include "Ability/Item/AbilityItemDataBase.h"
#include "Ability/Interaction/AbilityInteractionOptions.h"
#include "Common/Interaction/InteractionComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/MaterialBillboardComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

AInventoryPickupActor::AInventoryPickupActor()
{
    bReplicates = true;
    SetReplicateMovement(true);
    PrimaryActorTick.bCanEverTick = false;
    Collision = CreateDefaultSubobject<USphereComponent>(TEXT("PickupCollision"));
    Collision->SetSphereRadius(24.f);
    Collision->SetCollisionProfileName(PickupCollisionProfile);
    Collision->SetGenerateOverlapEvents(true);
    SetRootComponent(Collision);
    Interaction = CreateDefaultSubobject<UInteractionComponent>(TEXT("Interaction"));
    Interaction->SetupAttachment(Collision);
    Interaction->SetGenerateOverlapEvents(false);
    Interaction->SetBoxExtent(FVector(120.f));
    Interaction->Options.Add(CreateDefaultSubobject<UInteractionOption_AbilityPickUp>(TEXT("PickupOption")));
    VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("PickupVisualRoot"));
    VisualRoot->SetupAttachment(Collision);
    IconPresentation = CreateDefaultSubobject<UWidgetComponent>(TEXT("PickupIconWidget"));
    IconPresentation->SetupAttachment(VisualRoot);
    IconPresentation->SetWidgetSpace(EWidgetSpace::Screen);
    IconPresentation->SetWidgetClass(UWidgetInventoryPickupIcon::StaticClass());
    IconPresentation->SetDrawSize(FVector2D(48.f));
    IconPresentation->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MaterialIconPresentation = CreateDefaultSubobject<UMaterialBillboardComponent>(TEXT("PickupMaterialIcon"));
    MaterialIconPresentation->SetupAttachment(VisualRoot);
    MaterialIconPresentation->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    StaticPresentation = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PickupStaticMesh"));
    StaticPresentation->SetupAttachment(VisualRoot);
    StaticPresentation->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SkeletalPresentation = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("PickupSkeletalMesh"));
    SkeletalPresentation->SetupAttachment(VisualRoot);
    SkeletalPresentation->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SkeletalPresentation->SetComponentTickEnabled(false);
}

void AInventoryPickupActor::BeginPlay()
{
    Super::BeginPlay();
    OnRep_Presentation();
}

void AInventoryPickupActor::EndPlay(const EEndPlayReason::Type InReason)
{
    if (System.IsValid())
    {
        System->OnCommitted.Remove(CommitHandle);
        System->OnDeleted.Remove(DeleteHandle);
    }
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(PresentationRetry);
    System.Reset();
    Super::EndPlay(InReason);
}

void AInventoryPickupActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AInventoryPickupActor, bPublished);
    DOREPLIFETIME(AInventoryPickupActor, InventoryId);
    DOREPLIFETIME(AInventoryPickupActor, PickupAction);
    DOREPLIFETIME(AInventoryPickupActor, PickupCollisionProfile);
    DOREPLIFETIME(AInventoryPickupActor, PublicItemId);
    DOREPLIFETIME(AInventoryPickupActor, PublicCount);
}

bool AInventoryPickupActor::BindInventory(UInventoryWorldSubsystem& InSystem, const FGuid& InInventoryId)
{
    if (!HasAuthority() || InSystem.GetWorld() != GetWorld() || !InInventoryId.IsValid()) return false;
    if (System.IsValid())
    {
        System->OnCommitted.Remove(CommitHandle);
        System->OnDeleted.Remove(DeleteHandle);
    }
    CommitHandle.Reset();
    DeleteHandle.Reset();
    System = &InSystem;
    InventoryId = InInventoryId;
    if (!RefreshFromSnapshot())
    {
        System.Reset();
        InventoryId.Invalidate();
        return false;
    }
    CommitHandle = InSystem.OnCommitted.AddUObject(this, &ThisClass::HandleCommit);
    DeleteHandle = InSystem.OnDeleted.AddUObject(this, &ThisClass::HandleDeleted);
    return true;
}

void AInventoryPickupActor::InitializePresentation(const FGuid& InInventoryId, const FPrimaryAssetId& InItemId, int32 InCount)
{
    if (!HasAuthority()) return;
    InventoryId = InInventoryId;
    PublicItemId = InItemId;
    PublicCount = FMath::Max(0, InCount);
    OnRep_Presentation();
    ForceNetUpdate();
}

FGuid AInventoryPickupActor::GetInventoryId() const { return InventoryId; }
int32 AInventoryPickupActor::GetPublicCount() const { return PublicCount; }
FPrimaryAssetId AInventoryPickupActor::GetPublicItemId() const { return PublicItemId; }
FName AInventoryPickupActor::GetPickupAction() const { return PickupAction; }
UInteractionComponent* AInventoryPickupActor::GetInteractionComponent() const { return Interaction; }
EInteractAgentType AInventoryPickupActor::GetInteractAgentType() const { return EInteractAgentType::Passivity; }

void AInventoryPickupActor::HandleCommit(const FInventoryChangeSet& InChanges)
{
    if (InChanges.Revisions.ContainsByPredicate([this](const FInventoryRevision& Revision) { return Revision.InventoryId == InventoryId; }))
        RefreshFromSnapshot();
}

void AInventoryPickupActor::HandleDeleted(const FGuid& InInventoryId)
{
    if (InInventoryId != InventoryId) return;
    PublicCount = 0;
    OnRep_Presentation();
    ForceNetUpdate();
}

bool AInventoryPickupActor::RefreshFromSnapshot()
{
    FInventorySnapshot Snapshot;
    FString Error;
    if (!System.IsValid() || !System->GetSnapshot(InventoryId, Snapshot, Error) || Snapshot.Slots.Num() != 1) return false;
    InitializePresentation(InventoryId, Snapshot.Slots[0].Item.ItemId, Snapshot.Slots[0].Item.Count);
    return true;
}

void AInventoryPickupActor::OnRep_Presentation()
{
    const bool bAvailable = bPublished && PublicCount > 0 && PublicItemId.IsValid() && InventoryId.IsValid();
    Collision->SetCollisionProfileName(PickupCollisionProfile);
    SetActorHiddenInGame(!bAvailable);
    SetActorEnableCollision(bAvailable);
    Interaction->SetInteractable(bAvailable && !PickupAction.IsNone());
    Interaction->NotifyOptionsChanged();
    RefreshVisual();
}

void AInventoryPickupActor::RefreshVisual()
{
    if (!GetWorld() || GetWorld()->GetNetMode() == NM_DedicatedServer || PublicCount <= 0) return;
    const UInventoryWorldSubsystem* WorldSystem = GetWorld()->GetSubsystem<UInventoryWorldSubsystem>();
    const UInventoryItemAssetCatalog* Assets = WorldSystem ? WorldSystem->GetItemAssets() : nullptr;
    if (!Assets || !Assets->IsInitialized())
    {
        // 循环等待不在回调中重置自身；达到期限后给出资产启动错误。
        if (PresentationDeadline == 0.0)
        {
            PresentationDeadline = FPlatformTime::Seconds() + 60.0;
            GetWorld()->GetTimerManager().SetTimer(PresentationRetry, this, &ThisClass::RefreshVisual, 0.1f, true);
        }
        if (FPlatformTime::Seconds() >= PresentationDeadline)
        {
            GetWorld()->GetTimerManager().ClearTimer(PresentationRetry);
            UE_LOG(LogTemp, Error, TEXT("拾取表现等待物品目录超时: %s"), *GetPathName());
        }
        return;
    }
    GetWorld()->GetTimerManager().ClearTimer(PresentationRetry);
    const UAbilityItemDataBase* Definition = Assets->Find(PublicItemId);
    if (!Definition)
    {
        UE_LOG(LogTemp, Error, TEXT("拾取投影引用未注册物品: %s"), *PublicItemId.ToString());
        return;
    }
    const FInventoryPickupPresentation Visual = Definition ? Definition->PickupPresentation : FInventoryPickupPresentation();
    VisualRoot->SetRelativeTransform(Visual.LocalTransform);
    const bool bStatic = Visual.Visual == EInventoryPickupVisual::StaticMesh && Visual.StaticMesh;
    const bool bSkeletal = Visual.Visual == EInventoryPickupVisual::SkeletalMesh && Visual.SkeletalMesh;
    StaticPresentation->SetStaticMesh(bStatic ? Visual.StaticMesh.Get() : nullptr);
    StaticPresentation->SetVisibility(bStatic);
    SkeletalPresentation->SetSkeletalMesh(bSkeletal ? Visual.SkeletalMesh.Get() : nullptr);
    SkeletalPresentation->SetVisibility(bSkeletal);
    StaticPresentation->EmptyOverrideMaterials();
    SkeletalPresentation->EmptyOverrideMaterials();
    if (Visual.MaterialOverride)
    {
        if (bStatic) StaticPresentation->SetMaterial(0, Visual.MaterialOverride);
        if (bSkeletal) SkeletalPresentation->SetMaterial(0, Visual.MaterialOverride);
    }
    UMaterialInterface* IconMaterial = Cast<UMaterialInterface>(Definition->Icon);
    const bool bSurfaceIcon = !bStatic && !bSkeletal && IconMaterial && IconMaterial->GetMaterial()
        && IconMaterial->GetMaterial()->MaterialDomain == MD_Surface;
    MaterialIconPresentation->Elements.Reset();
    if (bSurfaceIcon)
    {
        MaterialIconPresentation->AddElement(IconMaterial, nullptr, false, Visual.IconSize * 0.5f, Visual.IconSize * 0.5f, nullptr);
    }
    MaterialIconPresentation->MarkRenderStateDirty();
    MaterialIconPresentation->SetVisibility(bSurfaceIcon);
    IconPresentation->SetVisibility(!bStatic && !bSkeletal && !bSurfaceIcon);
    IconPresentation->SetDrawSize(FVector2D(FMath::Clamp(Visual.IconSize, 8.f, 512.f)));
    IconPresentation->InitWidget();
    if (UWidgetInventoryPickupIcon* Widget = Cast<UWidgetInventoryPickupIcon>(IconPresentation->GetUserWidgetObject()))
        Widget->SetItemPresentation(Definition ? Definition->Icon : nullptr,
            Definition ? Definition->Name : FText::FromString(PublicItemId.ToString()), PublicCount);
    OnPickupPresentationChanged(PublicItemId, PublicCount);
}

bool AInventoryPickupActor::IsInteractable(IInteractionAgentInterface* InAgent)
{
    return bPublished && PublicCount > 0 && !PickupAction.IsNone() && IInteractionAgentInterface::IsInteractable(InAgent);
}

void AInventoryPickupActor::SetHighlighted(bool bInHighlighted)
{
    StaticPresentation->SetRenderCustomDepth(bInHighlighted);
    SkeletalPresentation->SetRenderCustomDepth(bInHighlighted);
    IconPresentation->SetTintColorAndOpacity(bInHighlighted ? FLinearColor(0.7f, 0.9f, 1.f) : FLinearColor::White);
}

void AInventoryPickupActor::OnEnterInteract(IInteractionAgentInterface* InAgent) { SetHighlighted(true); }
void AInventoryPickupActor::OnLeaveInteract(IInteractionAgentInterface* InAgent) { SetHighlighted(false); }

void AInventoryPickupActor::SetPickupAction(FName InAction)
{
    if (!HasAuthority()) return;
    PickupAction = InAction;
    OnRep_Presentation();
    ForceNetUpdate();
}

void AInventoryPickupActor::SetPickupCollisionProfile(FName InProfile)
{
    if (!HasAuthority() || InProfile.IsNone()) return;
    PickupCollisionProfile = InProfile;
    OnRep_Presentation();
    ForceNetUpdate();
}


void AInventoryPickupActor::SetPickupPublished(bool bInPublished)
{
    if (!HasAuthority()) return;
    bPublished = bInPublished;
    OnRep_Presentation();
    ForceNetUpdate();
}

#include "Ability/Item/AbilityItemBase.h"
#include "Ability/AbilityItemTypes.h"

#include "Ability/Inventory/InventoryItemAssetCatalog.h"
#include "Ability/Inventory/InventoryModule.h"
#include "Engine/World.h"

AAbilityItemBase::AAbilityItemBase()
{
    PrimaryActorTick.bCanEverTick = false;
    OwnerActor = nullptr;
}

void AAbilityItemBase::OnSpawn_Implementation(const FParameter& InParam)
{
    Super::OnSpawn_Implementation(InParam);
    if (const FAbilityItemSpawnParameter* Param = InParam.GetPtr<FAbilityItemSpawnParameter>())
    {
        InitializeProjection(Param->ItemId, Param->Level, Param->Owner);
    }
}

void AAbilityItemBase::OnDespawn_Implementation(EObjectDespawnMode InMode)
{
    ItemId = FPrimaryAssetId();
    ItemLevel = 0;
    OwnerActor = nullptr;
    Super::OnDespawn_Implementation(InMode);
}

bool AAbilityItemBase::InitializeProjection(const FPrimaryAssetId& InItemId, int32 InLevel, AActor* InAvatar)
{
    const UInventoryModule* Subsystem = GetWorld() ? UInventoryModule::GetPtr(GetWorld()) : nullptr;
    const FInventoryItemDefinition* Definition = Subsystem ? Subsystem->GetRegistry().Find(InItemId) : nullptr;
    if (!Definition || InLevel < 0 || InLevel > Definition->MaxLevel ||
        !Subsystem->GetItemAssets() || !Subsystem->GetItemAssets()->Find(InItemId) ||
        (InAvatar && InAvatar->GetWorld() != GetWorld()))
    {
        return false;
    }
    ItemId = InItemId;
    ItemLevel = InLevel;
    OwnerActor = InAvatar;
    SetOwner(InAvatar);
    return true;
}

const UAbilityItemDataBase* AAbilityItemBase::GetItemDefinition() const
{
    const UInventoryModule* Subsystem = GetWorld() ? UInventoryModule::GetPtr(GetWorld()) : nullptr;
    const UInventoryItemAssetCatalog* Catalog = Subsystem ? Subsystem->GetItemAssets() : nullptr;
    return Catalog ? Catalog->Find(ItemId) : nullptr;
}

const UAbilityItemDataBase& AAbilityItemBase::GetItemData() const
{
    const UAbilityItemDataBase* Definition = GetItemDefinition();
    checkf(Definition, TEXT("Item projection requires its preloaded definition: %s"), *ItemId.ToString());
    return *Definition;
}

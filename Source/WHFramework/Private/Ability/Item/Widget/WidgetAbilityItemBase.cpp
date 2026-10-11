#include "Ability/Item/Widget/WidgetAbilityItemBase.h"

#include "Ability/Inventory/InventoryItemAssetCatalog.h"
#include "Ability/Inventory/InventoryWorldSubsystem.h"
#include "Engine/World.h"

UWidgetAbilityItemBase::UWidgetAbilityItemBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
    SetIsFocusable(true);
    Item = FAbilityItem();
}

void UWidgetAbilityItemBase::OnSpawn_Implementation(const FParameter& InParam)
{
    Super::OnSpawn_Implementation(InParam);
}

void UWidgetAbilityItemBase::OnDespawn_Implementation(EObjectDespawnMode InMode)
{
    Super::OnDespawn_Implementation(InMode);
    Item = FAbilityItem();
}

void UWidgetAbilityItemBase::OnCreate(const FParameter& InParam)
{
    Super::OnCreate(InParam);
}

void UWidgetAbilityItemBase::OnInitialize(const FParameter& InParam)
{
    if (const FAbilityWidgetSpawnParameter* Parameter = InParam.GetPtr<FAbilityWidgetSpawnParameter>())
    {
        Item = Parameter->Item;
    }
    Super::OnInitialize(InParam);
}

void UWidgetAbilityItemBase::OnRefresh()
{
    Super::OnRefresh();
}

void UWidgetAbilityItemBase::OnDestroy(EObjectDespawnMode InMode)
{
    Super::OnDestroy(InMode);
    Item = FAbilityItem();
}

void UWidgetAbilityItemBase::SetItem(const FAbilityItem& InItem)
{
    Item = InItem;
    OnRefresh();
}

const UAbilityItemDataBase* UWidgetAbilityItemBase::FindItemData() const
{
    const UWorld* World = GetWorld();
    const UInventoryWorldSubsystem* System = World ? World->GetSubsystem<UInventoryWorldSubsystem>() : nullptr;
    const UInventoryItemAssetCatalog* Assets = System ? System->GetItemAssets() : nullptr;
    return Assets && !Item.IsEmpty() ? Assets->Find(Item.ItemId) : nullptr;
}

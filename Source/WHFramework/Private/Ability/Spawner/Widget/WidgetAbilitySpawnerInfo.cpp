#include "Ability/Spawner/Widget/WidgetAbilitySpawnerInfo.h"

#include "Ability/Inventory/InventoryItemAssetCatalog.h"
#include "Ability/Inventory/InventoryWorldSubsystem.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"

UWidgetAbilitySpawnerInfo::UWidgetAbilitySpawnerInfo(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
}

void UWidgetAbilitySpawnerInfo::NativeConstruct()
{
    Super::NativeConstruct();
    RefreshPresentation();
}

void UWidgetAbilitySpawnerInfo::InitAbilityItem(const FAbilityItem& InAbilityItem)
{
    Item = InAbilityItem;
    RefreshPresentation();
}

void UWidgetAbilitySpawnerInfo::RefreshPresentation()
{
    if (!ItemText)
    {
        return;
    }
    if (!Item.ItemId.IsValid())
    {
        ItemText->SetText(FText::GetEmpty());
        return;
    }
    const UInventoryWorldSubsystem* Subsystem = GetWorld() ? GetWorld()->GetSubsystem<UInventoryWorldSubsystem>() : nullptr;
    const UInventoryItemAssetCatalog* Catalog = Subsystem ? Subsystem->GetItemAssets() : nullptr;
    const UAbilityItemDataBase* Definition = Catalog ? Catalog->Find(Item.ItemId) : nullptr;
    const FText Name = Definition ? Definition->Name : FText::FromString(Item.ItemId.ToString());
    ItemText->SetText(FText::Format(NSLOCTEXT("Inventory", "SpawnerPresentation", "{0} ×{1} · Lv.{2}"),
        Name, FText::AsNumber(Item.Count), FText::AsNumber(Item.Level)));
}

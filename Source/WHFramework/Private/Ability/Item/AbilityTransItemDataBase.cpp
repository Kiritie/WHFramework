#include "Ability/Item/AbilityTransItemDataBase.h"
#include "Ability/Inventory/InventoryItemTypes.h"

UAbilityTransItemDataBase::UAbilityTransItemDataBase()
{
	Prices = TArray<FAbilityItem>();
	Upgrades = TArray<FAbilityItem>();
	Splits = TArray<FAbilityItem>();
}

void UAbilityTransItemDataBase::OnReset_Implementation()
{
	Super::OnReset_Implementation();
}

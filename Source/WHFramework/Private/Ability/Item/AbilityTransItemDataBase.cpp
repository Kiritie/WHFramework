#include "Ability/Item/AbilityTransItemDataBase.h"
#include "Ability/Inventory/InventoryItemTypes.h"

UAbilityTransItemDataBase::UAbilityTransItemDataBase()
{
	Prices = TArray<FAbilityItem>();
}

void UAbilityTransItemDataBase::OnReset_Implementation()
{
	Super::OnReset_Implementation();
}

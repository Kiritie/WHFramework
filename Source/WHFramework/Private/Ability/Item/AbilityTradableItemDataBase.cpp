#include "Ability/Item/AbilityTradableItemDataBase.h"

UAbilityTradableItemDataBase::UAbilityTradableItemDataBase()
{
	Prices = TArray<FAbilityItem>();
}

void UAbilityTradableItemDataBase::ResetData_Implementation()
{
	Super::ResetData_Implementation();
}

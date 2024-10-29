#include "Ability/Item/AbilityTransactionItemDataBase.h"

UAbilityTransactionItemDataBase::UAbilityTransactionItemDataBase()
{
	Prices = TArray<FAbilityItem>();
}

void UAbilityTransactionItemDataBase::OnReset_Implementation()
{
	Super::OnReset_Implementation();
}

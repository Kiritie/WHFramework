// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/Inventory/Slot/AbilityInventoryEquipSlotBase.h"

#include "Ability/Inventory/AbilityInventoryAgentInterface.h"

UAbilityInventoryEquipSlotBase::UAbilityInventoryEquipSlotBase()
{
}

void UAbilityInventoryEquipSlotBase::OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnSpawn_Implementation(InOwner, InParams);
}

void UAbilityInventoryEquipSlotBase::OnDespawn_Implementation(bool bRecovery)
{
	Super::OnDespawn_Implementation(bRecovery);
}

void UAbilityInventoryEquipSlotBase::OnInitialize(UAbilityInventoryBase* InInventory, EAbilityItemType InLimitType, ESlotSplitType InSplitType, int32 InSlotIndex)
{
	Super::OnInitialize(InInventory, InLimitType, InSplitType, InSlotIndex);
}

bool UAbilityInventoryEquipSlotBase::MatchItemLimit(FAbilityItem InItem) const
{
	return Super::MatchItemLimit(InItem);
}

void UAbilityInventoryEquipSlotBase::Refresh()
{
	Super::Refresh();
}

void UAbilityInventoryEquipSlotBase::OnItemPreChange(FAbilityItem& InNewItem)
{
	Super::OnItemPreChange(InNewItem);

	if(IsEmpty()) return;

	DeactiveItem(true);
}

void UAbilityInventoryEquipSlotBase::OnItemChanged(FAbilityItem& InOldItem)
{
	Super::OnItemChanged(InOldItem);

	if(IsEmpty()) return;

	ActiveItem(true);
}

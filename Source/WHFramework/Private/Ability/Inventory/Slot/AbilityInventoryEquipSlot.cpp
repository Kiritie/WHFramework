// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/Inventory/Slot/AbilityInventoryEquipSlot.h"

#include "Ability/Inventory/AbilityInventoryAgentInterface.h"
#include "Ability/Inventory/AbilityInventoryBase.h"

UAbilityInventoryEquipSlot::UAbilityInventoryEquipSlot()
{
}

void UAbilityInventoryEquipSlot::OnInitialize(UAbilityInventoryBase* InInventory, EAbilityItemType InLimitType, ESlotSplitType InSplitType, int32 InSlotIndex)
{
	Super::OnInitialize(InInventory, InLimitType, InSplitType, InSlotIndex);
}

void UAbilityInventoryEquipSlot::OnSpawn_Implementation(const TArray<FParameter>& InParams)
{
	Super::OnSpawn_Implementation(InParams);
}

void UAbilityInventoryEquipSlot::OnDespawn_Implementation(bool bRecovery)
{
	Super::OnDespawn_Implementation(bRecovery);
}

bool UAbilityInventoryEquipSlot::CheckSlot(FAbilityItem& InItem) const
{
	return Super::CheckSlot(InItem);
}

void UAbilityInventoryEquipSlot::Refresh()
{
	Super::Refresh();
}

void UAbilityInventoryEquipSlot::OnItemPreChange(FAbilityItem& InNewItem)
{
	Super::OnItemPreChange(InNewItem);
	CancelItem(true);
	
	if(auto Agent = GetInventory()->GetOwnerAgent())
	{
		Agent->OnDischargeItem(Item);
	}
}

void UAbilityInventoryEquipSlot::OnItemChanged(FAbilityItem& InOldItem)
{
	Super::OnItemChanged(InOldItem);
	ActiveItem(true);

	if(auto Agent = GetInventory()->GetOwnerAgent())
	{
		Agent->OnAssembleItem(Item);
	}
}

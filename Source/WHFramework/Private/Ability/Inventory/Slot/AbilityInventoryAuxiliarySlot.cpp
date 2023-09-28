// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Ability/Inventory/Slot/AbilityInventoryAuxiliarySlot.h"
#include "Ability/Inventory/AbilityInventoryBase.h"
#include "Ability/Inventory/AbilityInventoryAgentInterface.h"
#include "Ability/Inventory/Slot/AbilityInventorySlot.h"

UAbilityInventoryAuxiliarySlot::UAbilityInventoryAuxiliarySlot()
{
}

void UAbilityInventoryAuxiliarySlot::OnInitialize(UAbilityInventoryBase* InInventory, EAbilityItemType InLimitType, ESlotSplitType InSplitType, int32 InSlotIndex)
{
	Super::OnInitialize(InInventory, InLimitType, InSplitType, InSlotIndex);
}

void UAbilityInventoryAuxiliarySlot::OnItemChanged(FAbilityItem& InOldItem)
{
	Super::OnItemChanged(InOldItem);

	if(auto InventoryAgent = Inventory->GetOwnerAgent())
	{
		InventoryAgent->OnAuxiliaryItem(GetItem());
	}
}

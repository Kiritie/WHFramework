// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Ability/Inventory/Slot/AbilityInventoryAuxiliarySlotSlot.h"
#include "Ability/Inventory/AbilityInventoryBase.h"
#include "Ability/Inventory/AbilityInventoryAgentInterface.h"
#include "Ability/Inventory/Slot/AbilityInventorySlotBase.h"

UAbilityInventoryAuxiliarySlotSlot::UAbilityInventoryAuxiliarySlotSlot()
{
}

void UAbilityInventoryAuxiliarySlotSlot::OnInitialize(UAbilityInventoryBase* InInventory, EAbilityItemType InLimitType, ESlotSplitType InSplitType, int32 InSlotIndex)
{
	Super::OnInitialize(InInventory, InLimitType, InSplitType, InSlotIndex);
}

void UAbilityInventoryAuxiliarySlotSlot::OnItemChanged(FAbilityItem& InOldItem)
{
	Super::OnItemChanged(InOldItem);

	if(auto Agent = Inventory->GetOwnerAgent())
	{
		Agent->OnAuxiliaryItem(GetItem());
	}
}

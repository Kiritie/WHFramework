// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Ability/Inventory/Slot/AbilityInventoryAuxiliarySlot.h"
#include "Ability/Inventory/AbilityInventoryBase.h"
#include "Ability/Inventory/AbilityInventoryAgentInterface.h"
#include "Ability/Inventory/Slot/AbilityInventorySlot.h"

UAbilityInventoryAuxiliarySlot::UAbilityInventoryAuxiliarySlot()
{
}

void UAbilityInventoryAuxiliarySlot::OnInitialize(UAbilityInventoryBase* InInventory, FAbilityItem InItem, EAbilityItemType InLimitType /* = EAbilityItemType::None */, ESplitSlotType InSplitType /*= ESplitSlotType::Default*/)
{
	Super::OnInitialize(InInventory, InItem, InLimitType, InSplitType);
}

void UAbilityInventoryAuxiliarySlot::OnItemChanged(FAbilityItem& InOldItem)
{
	Super::OnItemChanged(InOldItem);

	if(auto InventoryAgent = Inventory->GetOwnerAgent())
	{
		InventoryAgent->OnAuxiliaryItem(GetItem());
	}
}

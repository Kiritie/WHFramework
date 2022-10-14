// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Ability/Inventory/Slot/InventoryAuxiliarySlot.h"
#include "Ability/Inventory/Inventory.h"
#include "Ability/Inventory/Slot/InventorySlot.h"

UInventoryAuxiliarySlot::UInventoryAuxiliarySlot()
{
}

void UInventoryAuxiliarySlot::OnInitialize(UInventory* InInventory, FAbilityItem InItem, EAbilityItemType InLimitType /* = EAbilityItemType::None */, ESplitSlotType InSplitType /*= ESplitSlotType::Default*/)
{
	Super::OnInitialize(InInventory, InItem, InLimitType, InSplitType);
}

void UInventoryAuxiliarySlot::OnItemChanged(FAbilityItem& InOldItem)
{
	Super::OnItemChanged(InOldItem);

	if(auto InventoryAgent = Inventory->GetOwnerAgent())
	{
		InventoryAgent->OnAuxiliaryItem(GetItem());
	}
}

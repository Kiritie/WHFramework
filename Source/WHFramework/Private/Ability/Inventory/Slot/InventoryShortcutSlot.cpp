// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Ability/Inventory/Slot/InventoryShortcutSlot.h"

UInventoryShortcutSlot::UInventoryShortcutSlot()
{
}

void UInventoryShortcutSlot::OnInitialize(UInventory* InInventory, FAbilityItem InItem, EAbilityItemType InLimitType /* = EAbilityItemType::None */, ESplitSlotType InSplitType /*= ESplitSlotType::Default*/)
{
	Super::OnInitialize(InInventory, InItem, InLimitType, InSplitType);
}

void UInventoryShortcutSlot::Refresh()
{
	Super::Refresh();
}

// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Ability/Inventory/Slot/AbilityInventoryShortcutSlot.h"

UAbilityInventoryShortcutSlot::UAbilityInventoryShortcutSlot()
{
}

void UAbilityInventoryShortcutSlot::OnInitialize(UAbilityInventoryBase* InInventory, EAbilityItemType InLimitType, ESlotSplitType InSplitType, int32 InSlotIndex)
{
	Super::OnInitialize(InInventory, InLimitType, InSplitType, InSlotIndex);
}

void UAbilityInventoryShortcutSlot::Refresh()
{
	Super::Refresh();
}

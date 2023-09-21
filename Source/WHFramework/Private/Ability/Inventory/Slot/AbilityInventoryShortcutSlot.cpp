// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Ability/Inventory/Slot/AbilityInventoryShortcutSlot.h"

UAbilityInventoryShortcutSlot::UAbilityInventoryShortcutSlot()
{
}

void UAbilityInventoryShortcutSlot::OnInitialize(UAbilityInventoryBase* InInventory, FAbilityItem InItem, EAbilityItemType InLimitType /* = EAbilityItemType::None */, ESplitSlotType InSplitType /*= ESplitSlotType::Default*/)
{
	Super::OnInitialize(InInventory, InItem, InLimitType, InSplitType);
}

void UAbilityInventoryShortcutSlot::Refresh()
{
	Super::Refresh();
}

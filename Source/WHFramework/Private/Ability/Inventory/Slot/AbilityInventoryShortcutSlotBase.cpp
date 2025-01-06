// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Ability/Inventory/Slot/AbilityInventoryShortcutSlotBase.h"

UAbilityInventoryShortcutSlotBase::UAbilityInventoryShortcutSlotBase()
{
}

void UAbilityInventoryShortcutSlotBase::OnInitialize(UAbilityInventoryBase* InInventory, EAbilityItemType InLimitType, ESlotSplitType InSplitType, int32 InSlotIndex)
{
	Super::OnInitialize(InInventory, InLimitType, InSplitType, InSlotIndex);
}

void UAbilityInventoryShortcutSlotBase::Refresh()
{
	Super::Refresh();
}

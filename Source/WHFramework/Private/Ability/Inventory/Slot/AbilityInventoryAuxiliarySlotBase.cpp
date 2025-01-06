// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Ability/Inventory/Slot/AbilityInventoryAuxiliarySlotBase.h"
#include "Ability/Inventory/AbilityInventoryBase.h"
#include "Ability/Inventory/AbilityInventoryAgentInterface.h"
#include "Ability/Inventory/Slot/AbilityInventorySlotBase.h"

UAbilityInventoryAuxiliarySlotBase::UAbilityInventoryAuxiliarySlotBase()
{
}

void UAbilityInventoryAuxiliarySlotBase::OnInitialize(UAbilityInventoryBase* InInventory, EAbilityItemType InLimitType, ESlotSplitType InSplitType, int32 InSlotIndex)
{
	Super::OnInitialize(InInventory, InLimitType, InSplitType, InSlotIndex);
}

void UAbilityInventoryAuxiliarySlotBase::OnItemChanged(FAbilityItem& InOldItem, bool bBroadcast)
{
	Super::OnItemChanged(InOldItem, bBroadcast);

	if(auto Agent = Inventory->GetOwnerAgent())
	{
		Agent->OnAuxiliaryItem(GetItem());
	}
}

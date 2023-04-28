// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Ability/Inventory/Slot/InventorySkillSlot.h"

#include "Ability/Inventory/Inventory.h"
#include "Ability/Inventory/Slot/InventorySlot.h"
#include "Ability/Item/Skill/AbilitySkillDataBase.h"

UInventorySkillSlot::UInventorySkillSlot()
{
}

void UInventorySkillSlot::OnInitialize(UInventory* InInventory, FAbilityItem InItem, EAbilityItemType InLimitType /* = EAbilityItemType::None */, ESplitSlotType InSplitType /*= ESplitSlotType::Default*/)
{
	Super::OnInitialize(InInventory, InItem, InLimitType, InSplitType);
}

void UInventorySkillSlot::OnItemPreChange(FAbilityItem& InNewItem)
{
	Super::OnItemPreChange(InNewItem);
	if(Item.IsValid() && Item.GetData<UAbilitySkillDataBase>().SkillMode == ESkillMode::Passive)
	{
		CancelItem(true);
	}
}

void UInventorySkillSlot::OnItemChanged(FAbilityItem& InOldItem)
{
	Super::OnItemChanged(InOldItem);
	if(Item.IsValid() && Item.GetData<UAbilitySkillDataBase>().SkillMode == ESkillMode::Passive)
	{
		ActiveItem(true);
	}
}

bool UInventorySkillSlot::ActiveItem(bool bPassive /*= false*/)
{
	return Super::ActiveItem(bPassive);
}

void UInventorySkillSlot::CancelItem(bool bPassive /*= false*/)
{
	return Super::CancelItem(bPassive);
}

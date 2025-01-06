// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Ability/Inventory/Slot/AbilityInventorySkillSlotBase.h"

#include "Ability/Inventory/AbilityInventoryBase.h"
#include "Ability/Inventory/Slot/AbilityInventorySlotBase.h"
#include "Ability/Item/Skill/AbilitySkillDataBase.h"

UAbilityInventorySkillSlotBase::UAbilityInventorySkillSlotBase()
{
}

void UAbilityInventorySkillSlotBase::OnInitialize(UAbilityInventoryBase* InInventory, EAbilityItemType InLimitType, ESlotSplitType InSplitType, int32 InSlotIndex)
{
	Super::OnInitialize(InInventory, InLimitType, InSplitType, InSlotIndex);
}

void UAbilityInventorySkillSlotBase::OnItemPreChange(FAbilityItem& InNewItem, bool bBroadcast)
{
	Super::OnItemPreChange(InNewItem, bBroadcast);
	
	if(Item.IsValid() && Item.GetData<UAbilitySkillDataBase>().SkillMode == EAbilitySkillMode::Passive)
	{
		DeactiveItem(true);
	}
}

void UAbilityInventorySkillSlotBase::OnItemChanged(FAbilityItem& InOldItem, bool bBroadcast)
{
	Super::OnItemChanged(InOldItem, bBroadcast);
	
	if(Item.IsValid() && Item.GetData<UAbilitySkillDataBase>().SkillMode == EAbilitySkillMode::Passive)
	{
		ActiveItem(true);
	}
}

bool UAbilityInventorySkillSlotBase::ActiveItem(bool bPassive /*= false*/)
{
	return Super::ActiveItem(bPassive);
}

void UAbilityInventorySkillSlotBase::DeactiveItem(bool bPassive /*= false*/)
{
	return Super::DeactiveItem(bPassive);
}

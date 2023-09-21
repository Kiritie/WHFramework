// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Ability/Character/AbilityCharacterInventoryBase.h"

#include "Ability/Inventory/Slot/AbilityInventorySkillSlot.h"

UAbilityCharacterInventoryBase::UAbilityCharacterInventoryBase()
{
	
}

void UAbilityCharacterInventoryBase::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	Super::LoadData(InSaveData, InPhase);
}

FSaveData* UAbilityCharacterInventoryBase::ToData(bool bRefresh)
{
	return Super::ToData(bRefresh);
}

void UAbilityCharacterInventoryBase::UnloadData(EPhase InPhase)
{
	Super::UnloadData(InPhase);
}

FQueryItemInfo UAbilityCharacterInventoryBase::QueryItemByRange(EQueryItemType InActionType, FAbilityItem InItem, int32 InStartIndex, int32 InEndIndex)
{
	return Super::QueryItemByRange(InActionType, InItem, InStartIndex, InEndIndex);
}

void UAbilityCharacterInventoryBase::DiscardAllItem()
{
	Super::DiscardAllItem();
}

void UAbilityCharacterInventoryBase::ClearAllItem()
{
	Super::ClearAllItem();
}

UAbilityInventorySkillSlot* UAbilityCharacterInventoryBase::GetSkillSlotByID(const FPrimaryAssetId& InSkillID)
{
	auto SkillSlots = GetSplitSlots<UAbilityInventorySkillSlot>(ESplitSlotType::Skill);
	for (int32 i = 0; i < SkillSlots.Num(); i++)
	{
		if(SkillSlots[i]->GetItem().ID == InSkillID)
		{
			return SkillSlots[i];
		}
	}
	return nullptr;
}

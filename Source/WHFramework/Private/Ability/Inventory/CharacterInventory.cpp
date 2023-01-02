// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Ability/Inventory/CharacterInventory.h"
#include "Ability/Inventory/Slot/InventorySkillSlot.h"

UCharacterInventory::UCharacterInventory()
{
	
}

void UCharacterInventory::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	Super::LoadData(InSaveData, InPhase);
}

FSaveData* UCharacterInventory::ToData()
{
	return Super::ToData();
}

void UCharacterInventory::UnloadData(EPhase InPhase)
{
	Super::UnloadData(InPhase);
}

FQueryItemInfo UCharacterInventory::QueryItemByRange(EQueryItemType InActionType, FAbilityItem InItem, int32 InStartIndex, int32 InEndIndex)
{
	return Super::QueryItemByRange(InActionType, InItem, InStartIndex, InEndIndex);
}

void UCharacterInventory::DiscardAllItem()
{
	Super::DiscardAllItem();
}

void UCharacterInventory::ClearAllItem()
{
	Super::ClearAllItem();
}

UInventorySkillSlot* UCharacterInventory::GetSkillSlotByID(const FPrimaryAssetId& InSkillID)
{
	auto SkillSlots = GetSplitSlots<UInventorySkillSlot>(ESplitSlotType::Skill);
	for (int32 i = 0; i < SkillSlots.Num(); i++)
	{
		if(SkillSlots[i]->GetItem().ID == InSkillID)
		{
			return SkillSlots[i];
		}
	}
	return nullptr;
}

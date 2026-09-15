// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Ability/Character/AbilityCharacterInventoryBase.h"

#include "Ability/Inventory/Slot/AbilityInventorySkillSlotBase.h"

UAbilityCharacterInventoryBase::UAbilityCharacterInventoryBase()
{
	
}

void UAbilityCharacterInventoryBase::LoadData(const FParameter& InSaveData, EPhase InPhase)
{
	Super::LoadData(InSaveData, InPhase);
}

FParameter UAbilityCharacterInventoryBase::ToData()
{
	return Super::ToData();
}

void UAbilityCharacterInventoryBase::UnloadData(EPhase InPhase)
{
	Super::UnloadData(InPhase);
}

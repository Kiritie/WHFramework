// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Ability/Vitality/AbilityVitalityInventoryBase.h"

UAbilityVitalityInventoryBase::UAbilityVitalityInventoryBase()
{
	
}

void UAbilityVitalityInventoryBase::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	Super::LoadData(InSaveData, InPhase);
}

FSaveData* UAbilityVitalityInventoryBase::ToData()
{
	return Super::ToData();
}

void UAbilityVitalityInventoryBase::UnloadData(EPhase InPhase)
{
	Super::UnloadData(InPhase);
}

void UAbilityVitalityInventoryBase::DiscardAllItem()
{
	Super::DiscardAllItem();
}

void UAbilityVitalityInventoryBase::ClearAllItem()
{
	Super::ClearAllItem();
}

FItemQueryInfo UAbilityVitalityInventoryBase::QueryItemByRange(EItemQueryType InActionType, FAbilityItem InItem, int32 InStartIndex, int32 InEndIndex)
{
	return Super::QueryItemByRange(InActionType, InItem, InStartIndex, InEndIndex);
}

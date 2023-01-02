// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Ability/Inventory/VitalityInventory.h"

UVitalityInventory::UVitalityInventory()
{
	
}

void UVitalityInventory::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	Super::LoadData(InSaveData, InPhase);
}

FSaveData* UVitalityInventory::ToData()
{
	return Super::ToData();
}

void UVitalityInventory::UnloadData(EPhase InPhase)
{
	Super::UnloadData(InPhase);
}

void UVitalityInventory::DiscardAllItem()
{
	Super::DiscardAllItem();
}

void UVitalityInventory::ClearAllItem()
{
	Super::ClearAllItem();
}

FQueryItemInfo UVitalityInventory::QueryItemByRange(EQueryItemType InActionType, FAbilityItem InItem, int32 InStartIndex, int32 InEndIndex)
{
	return Super::QueryItemByRange(InActionType, InItem, InStartIndex, InEndIndex);
}

// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Ability/Pawn/AbilityPawnInventoryBase.h"

UAbilityPawnInventoryBase::UAbilityPawnInventoryBase()
{
	
}

void UAbilityPawnInventoryBase::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	Super::LoadData(InSaveData, InPhase);
}

FSaveData* UAbilityPawnInventoryBase::ToData(bool bRefresh)
{
	return Super::ToData(bRefresh);
}

void UAbilityPawnInventoryBase::UnloadData(EPhase InPhase)
{
	Super::UnloadData(InPhase);
}

void UAbilityPawnInventoryBase::DiscardAllItem()
{
	Super::DiscardAllItem();
}

void UAbilityPawnInventoryBase::ClearAllItem()
{
	Super::ClearAllItem();
}

FItemQueryInfo UAbilityPawnInventoryBase::QueryItemByRange(EItemQueryType InActionType, FAbilityItem InItem, int32 InStartIndex, int32 InEndIndex)
{
	return Super::QueryItemByRange(InActionType, InItem, InStartIndex, InEndIndex);
}

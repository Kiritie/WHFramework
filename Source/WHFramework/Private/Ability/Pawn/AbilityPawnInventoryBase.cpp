// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Ability/Pawn/AbilityPawnInventoryBase.h"

UAbilityPawnInventoryBase::UAbilityPawnInventoryBase()
{
	
}

void UAbilityPawnInventoryBase::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	Super::LoadData(InSaveData, InPhase);
}

FSaveData* UAbilityPawnInventoryBase::ToData()
{
	return Super::ToData();
}

void UAbilityPawnInventoryBase::UnloadData(EPhase InPhase)
{
	Super::UnloadData(InPhase);
}

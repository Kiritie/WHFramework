// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Ability/Pawn/AbilityPawnInventoryBase.h"

UAbilityPawnInventoryBase::UAbilityPawnInventoryBase()
{
	
}

void UAbilityPawnInventoryBase::LoadData(const FParameter& InSaveData, EPhase InPhase)
{
	Super::LoadData(InSaveData, InPhase);
}

FParameter UAbilityPawnInventoryBase::ToData()
{
	return Super::ToData();
}

void UAbilityPawnInventoryBase::UnloadData(EPhase InPhase)
{
	Super::UnloadData(InPhase);
}

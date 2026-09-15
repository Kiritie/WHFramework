// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Ability/Vitality/AbilityVitalityInventoryBase.h"

UAbilityVitalityInventoryBase::UAbilityVitalityInventoryBase()
{
	
}

void UAbilityVitalityInventoryBase::LoadData(const FParameter& InSaveData, EPhase InPhase)
{
	Super::LoadData(InSaveData, InPhase);
}

FParameter UAbilityVitalityInventoryBase::ToData()
{
	return Super::ToData();
}

void UAbilityVitalityInventoryBase::UnloadData(EPhase InPhase)
{
	Super::UnloadData(InPhase);
}

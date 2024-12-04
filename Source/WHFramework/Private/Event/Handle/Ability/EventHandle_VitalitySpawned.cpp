// Fill out your copyright notice in the Description page of Project Settings.

#include "Event/Handle/Ability/EventHandle_VitalitySpawned.h"

#include "Ability/Vitality/AbilityVitalityInterface.h"

UEventHandle_VitalitySpawned::UEventHandle_VitalitySpawned()
{
	Vitality = nullptr;
	Rescuer = nullptr;
}

void UEventHandle_VitalitySpawned::OnDespawn_Implementation(bool bRecovery)
{
	Super::OnDespawn_Implementation(bRecovery);

	Vitality = nullptr;
	Rescuer = nullptr;
}

void UEventHandle_VitalitySpawned::Parse_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		Vitality = InParams[0];
	}
	if(InParams.IsValidIndex(1))
	{
		Rescuer = InParams[1];
	}
}

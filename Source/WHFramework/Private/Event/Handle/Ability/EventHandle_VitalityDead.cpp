// Fill out your copyright notice in the Description page of Project Settings.

#include "Event/Handle/Ability/EventHandle_VitalityDead.h"

#include "Ability/Vitality/AbilityVitalityInterface.h"

UEventHandle_VitalityDead::UEventHandle_VitalityDead()
{
	Vitality = nullptr;
	Killer = nullptr;
}

void UEventHandle_VitalityDead::OnDespawn_Implementation(bool bRecovery)
{
	Super::OnDespawn_Implementation(bRecovery);

	Vitality = nullptr;
	Killer = nullptr;
}

void UEventHandle_VitalityDead::Parse_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		Vitality = InParams[0];
	}
	if(InParams.IsValidIndex(1))
	{
		Killer = InParams[1];
	}
}

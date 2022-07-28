// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Vitality/States/AbilityVitalityState_Default.h"

#include "Ability/Vitality/AbilityVitalityBase.h"

UAbilityVitalityState_Default::UAbilityVitalityState_Default()
{
	StateName = FName("Default");
}

void UAbilityVitalityState_Default::OnInitialize(UFSMComponent* InFSMComponent, int32 InStateIndex)
{
	Super::OnInitialize(InFSMComponent, InStateIndex);
}

bool UAbilityVitalityState_Default::OnEnterValidate(UFiniteStateBase* InLastFiniteState)
{
	return Super::OnEnterValidate(InLastFiniteState);
}

void UAbilityVitalityState_Default::OnEnter(UFiniteStateBase* InLastFiniteState)
{
	Super::OnEnter(InLastFiniteState);

	AAbilityVitalityBase* Vitality = GetAgent<AAbilityVitalityBase>();

	Vitality->SetHealth(Vitality->GetMaxHealth());
}

void UAbilityVitalityState_Default::OnRefresh()
{
	Super::OnRefresh();
}

void UAbilityVitalityState_Default::OnLeave(UFiniteStateBase* InNextFiniteState)
{
	Super::OnLeave(InNextFiniteState);
}

void UAbilityVitalityState_Default::OnTermination()
{
	Super::OnTermination();
}

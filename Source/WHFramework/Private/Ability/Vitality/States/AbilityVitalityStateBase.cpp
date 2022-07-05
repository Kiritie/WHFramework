// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Vitality/States/AbilityVitalityStateBase.h"

UAbilityVitalityStateBase::UAbilityVitalityStateBase()
{
	StateName = FName("AbilityVitalityStateBase");
}

void UAbilityVitalityStateBase::OnInitialize(UFSMComponent* InFSMComponent, int32 InStateIndex)
{
	Super::OnInitialize(InFSMComponent, InStateIndex);
}

bool UAbilityVitalityStateBase::OnValidate()
{
	return Super::OnValidate();
}

void UAbilityVitalityStateBase::OnEnter(UFiniteStateBase* InLastFiniteState)
{
	Super::OnEnter(InLastFiniteState);
}

void UAbilityVitalityStateBase::OnRefresh()
{
	Super::OnRefresh();
}

void UAbilityVitalityStateBase::OnLeave(UFiniteStateBase* InNextFiniteState)
{
	Super::OnLeave(InNextFiniteState);
}

void UAbilityVitalityStateBase::OnTermination()
{
	Super::OnTermination();
}

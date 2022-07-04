// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Vitality/States/AbilityVitalityState_Death.h"

#include "AbilitySystemComponent.h"
#include "Ability/Vitality/AbilityVitalityBase.h"

UAbilityVitalityState_Death::UAbilityVitalityState_Death()
{
	StateName = FName("Death");
}

void UAbilityVitalityState_Death::OnInitialize(UFSMComponent* InFSMComponent, int32 InStateIndex)
{
	Super::OnInitialize(InFSMComponent, InStateIndex);
}

void UAbilityVitalityState_Death::OnEnter(UFiniteStateBase* InLastFiniteState)
{
	Super::OnEnter(InLastFiniteState);

	GetAgent<AAbilityVitalityBase>()->GetAbilitySystemComponent()
	->AddLooseGameplayTag(GetAgent<AAbilityVitalityBase>()->GetVitalityData().DeadTag);
}

void UAbilityVitalityState_Death::OnRefresh()
{
	Super::OnRefresh();
}

void UAbilityVitalityState_Death::OnLeave(UFiniteStateBase* InNextFiniteState)
{
	Super::OnLeave(InNextFiniteState);

	GetAgent<AAbilityVitalityBase>()->GetAbilitySystemComponent()
	->RemoveLooseGameplayTag(GetAgent<AAbilityVitalityBase>()->GetVitalityData().DeadTag);
}

void UAbilityVitalityState_Death::OnTermination()
{
	Super::OnTermination();
}

// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Vitality/States/AbilityVitalityState_Death.h"

#include "AbilitySystemComponent.h"
#include "Ability/Vitality/AbilityVitalityBase.h"
#include "Ability/Vitality/AbilityVitalityDataBase.h"
#include "ObjectPool/ObjectPoolModuleBPLibrary.h"

UAbilityVitalityState_Death::UAbilityVitalityState_Death()
{
	StateName = FName("Death");
}

void UAbilityVitalityState_Death::OnInitialize(UFSMComponent* InFSMComponent, int32 InStateIndex)
{
	Super::OnInitialize(InFSMComponent, InStateIndex);
}

bool UAbilityVitalityState_Death::OnEnterValidate(UFiniteStateBase* InLastFiniteState)
{
	return Super::OnEnterValidate(InLastFiniteState);
}

void UAbilityVitalityState_Death::OnEnter(UFiniteStateBase* InLastFiniteState)
{
	Super::OnEnter(InLastFiniteState);

	AAbilityVitalityBase* Vitality = GetAgent<AAbilityVitalityBase>();

	Vitality->GetAbilitySystemComponent()->AddLooseGameplayTag(Vitality->GetVitalityData().DyingTag);

	if(Killer)
	{
		Killer->ModifyExp(Vitality->GetExp());
	}

	Vitality->SetExp(0);
	Vitality->SetHealth(0.f);

	DeathStart();
}

void UAbilityVitalityState_Death::OnRefresh()
{
	Super::OnRefresh();
}

void UAbilityVitalityState_Death::OnLeave(UFiniteStateBase* InNextFiniteState)
{
	Super::OnLeave(InNextFiniteState);

	Killer = nullptr;

	AAbilityVitalityBase* Vitality = GetAgent<AAbilityVitalityBase>();

	Vitality->GetAbilitySystemComponent()->RemoveLooseGameplayTag(Vitality->GetVitalityData().DyingTag);
	Vitality->GetAbilitySystemComponent()->RemoveLooseGameplayTag(Vitality->GetVitalityData().DeadTag);
}

void UAbilityVitalityState_Death::OnTermination()
{
	Super::OnTermination();
}

void UAbilityVitalityState_Death::DeathStart()
{
	DeathEnd();
}

void UAbilityVitalityState_Death::DeathEnd()
{
	AAbilityVitalityBase* Vitality = GetAgent<AAbilityVitalityBase>();
	
	Vitality->GetAbilitySystemComponent()->RemoveLooseGameplayTag(Vitality->GetVitalityData().DyingTag);
	
	Vitality->GetAbilitySystemComponent()->AddLooseGameplayTag(Vitality->GetVitalityData().DeadTag);

	UObjectPoolModuleBPLibrary::DespawnObject(Vitality);
}

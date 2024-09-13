// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Vitality/States/AbilityVitalityState_Death.h"

#include "AbilitySystemComponent.h"
#include "Ability/Vitality/AbilityVitalityBase.h"
#include "Ability/Vitality/AbilityVitalityDataBase.h"
#include "ObjectPool/ObjectPoolModuleStatics.h"

UAbilityVitalityState_Death::UAbilityVitalityState_Death()
{
	StateName = FName("Death");
}

void UAbilityVitalityState_Death::OnInitialize(UFSMComponent* InFSM, int32 InStateIndex)
{
	Super::OnInitialize(InFSM, InStateIndex);
}

bool UAbilityVitalityState_Death::OnPreEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	return Super::OnPreEnter(InLastState, InParams);
}

void UAbilityVitalityState_Death::OnEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	Super::OnEnter(InLastState, InParams);

	if(InParams.IsValidIndex(0))
	{
		Killer = InParams[0].GetPointerValue<IAbilityVitalityInterface>();
	}

	AAbilityVitalityBase* Vitality = GetAgent<AAbilityVitalityBase>();

	Vitality->GetAbilitySystemComponent()->AddLooseGameplayTag(GameplayTags::StateTag_Vitality_Dying);

	if(Killer && Killer != Vitality)
	{
		Killer->ModifyExp(Vitality->GetLevelA() * 5.f);
	}

	Vitality->SetExp(0);
	Vitality->SetHealth(0.f);

	DeathStart();
}

void UAbilityVitalityState_Death::OnRefresh(float DeltaSeconds)
{
	Super::OnRefresh(DeltaSeconds);
}

void UAbilityVitalityState_Death::OnLeave(UFiniteStateBase* InNextState)
{
	Super::OnLeave(InNextState);

	Killer = nullptr;

	AAbilityVitalityBase* Vitality = GetAgent<AAbilityVitalityBase>();

	Vitality->GetAbilitySystemComponent()->RemoveLooseGameplayTag(GameplayTags::StateTag_Vitality_Dying);
	Vitality->GetAbilitySystemComponent()->RemoveLooseGameplayTag(GameplayTags::StateTag_Vitality_Dead);
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
	
	Vitality->GetAbilitySystemComponent()->RemoveLooseGameplayTag(GameplayTags::StateTag_Vitality_Dying);
	
	Vitality->GetAbilitySystemComponent()->AddLooseGameplayTag(GameplayTags::StateTag_Vitality_Dead);

	UObjectPoolModuleStatics::DespawnObject(Vitality);
}

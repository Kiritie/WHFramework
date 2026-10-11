// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Vitality/States/AbilityVitalityState_Walk.h"
#include "Ability/Vitality/AbilityVitalityBase.h"
#include "ObjectPool/ObjectPoolModuleStatics.h"

UAbilityVitalityState_Walk::UAbilityVitalityState_Walk()
{
	StateName = FName("Active");
}

void UAbilityVitalityState_Walk::OnInitialize(UFSMComponent* InFSM, int32 InStateIndex)
{
	Super::OnInitialize(InFSM, InStateIndex);
}

bool UAbilityVitalityState_Walk::OnPreEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	return Super::OnPreEnter(InLastState, InParams);
}

void UAbilityVitalityState_Walk::OnEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	Super::OnEnter(InLastState, InParams);

	AAbilityVitalityBase* Vitality = GetAgent<AAbilityVitalityBase>();

	SetStateTag(GameplayTags::State_Vitality_Active, true);

}

void UAbilityVitalityState_Walk::OnRefresh(float DeltaSeconds)
{
	Super::OnRefresh(DeltaSeconds);
}

void UAbilityVitalityState_Walk::OnLeave(UFiniteStateBase* InNextState)
{
	Super::OnLeave(InNextState);

	AAbilityVitalityBase* Vitality = GetAgent<AAbilityVitalityBase>();

	SetStateTag(GameplayTags::State_Vitality_Active, false);

}

void UAbilityVitalityState_Walk::OnTermination()
{
	Super::OnTermination();
}

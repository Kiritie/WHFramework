// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Vitality/States/AbilityVitalityState_Default.h"

#include "Ability/Vitality/AbilityVitalityBase.h"

UAbilityVitalityState_Default::UAbilityVitalityState_Default()
{
	StateName = FName("Default");
}

void UAbilityVitalityState_Default::OnInitialize(UFSMComponent* InFSM, int32 InStateIndex)
{
	Super::OnInitialize(InFSM, InStateIndex);
}

bool UAbilityVitalityState_Default::OnEnterValidate(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	return Super::OnEnterValidate(InLastState, InParams);
}

void UAbilityVitalityState_Default::OnEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	Super::OnEnter(InLastState, InParams);

	AAbilityVitalityBase* Vitality = GetAgent<AAbilityVitalityBase>();

	Vitality->ResetData();
}

void UAbilityVitalityState_Default::OnRefresh(float DeltaSeconds)
{
	Super::OnRefresh(DeltaSeconds);
}

void UAbilityVitalityState_Default::OnLeave(UFiniteStateBase* InNextState)
{
	Super::OnLeave(InNextState);
}

void UAbilityVitalityState_Default::OnTermination()
{
	Super::OnTermination();
}

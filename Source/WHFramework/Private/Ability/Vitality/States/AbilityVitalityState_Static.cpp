// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Vitality/States/AbilityVitalityState_Static.h"
#include "Ability/AbilityActionTypes.h"

#include "AbilitySystemComponent.h"
#include "Ability/Vitality/AbilityVitalityBase.h"
#include "Common/Interaction/InteractionComponent.h"
#include "Components/CapsuleComponent.h"

UAbilityVitalityState_Static::UAbilityVitalityState_Static()
{
	StateName = FName("Static");
	StateDomain = TEXT("Control");
}

void UAbilityVitalityState_Static::OnInitialize(UFSMComponent* InFSM, int32 InStateIndex)
{
	Super::OnInitialize(InFSM, InStateIndex);
}

bool UAbilityVitalityState_Static::OnPreEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	return Super::OnPreEnter(InLastState, InParams);
}

void UAbilityVitalityState_Static::OnEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	Super::OnEnter(InLastState, InParams);

	AAbilityVitalityBase* Vitality = GetAgent<AAbilityVitalityBase>();

	SetStateTag(AbilityActionTags::StaticControl, true);

}

void UAbilityVitalityState_Static::OnRefresh(float DeltaSeconds)
{
	Super::OnRefresh(DeltaSeconds);
}

void UAbilityVitalityState_Static::OnLeave(UFiniteStateBase* InNextState)
{
	Super::OnLeave(InNextState);

	AAbilityVitalityBase* Vitality = GetAgent<AAbilityVitalityBase>();

	SetStateTag(AbilityActionTags::StaticControl, false);

}

void UAbilityVitalityState_Static::OnTermination()
{
	Super::OnTermination();
}

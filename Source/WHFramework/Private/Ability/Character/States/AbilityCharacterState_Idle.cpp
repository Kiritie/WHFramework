// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Character/States/AbilityCharacterState_Idle.h"

#include "Ability/Character/AbilityCharacterBase.h"

UAbilityCharacterState_Idle::UAbilityCharacterState_Idle()
{
	StateName = FName("Idle");
}

void UAbilityCharacterState_Idle::OnInitialize(UFSMComponent* InFSMComponent, int32 InStateIndex)
{
	Super::OnInitialize(InFSMComponent, InStateIndex);
}

bool UAbilityCharacterState_Idle::OnValidate()
{
	return Super::OnValidate();
}

void UAbilityCharacterState_Idle::OnEnter(UFiniteStateBase* InLastFiniteState)
{
	Super::OnEnter(InLastFiniteState);
}

void UAbilityCharacterState_Idle::OnRefresh()
{
	Super::OnRefresh();
}

void UAbilityCharacterState_Idle::OnLeave(UFiniteStateBase* InNextFiniteState)
{
	Super::OnLeave(InNextFiniteState);
}

void UAbilityCharacterState_Idle::OnTermination()
{
	Super::OnTermination();
}

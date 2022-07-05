// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Character/States/AbilityCharacterState_Walk.h"

#include "Ability/Character/AbilityCharacterBase.h"

UAbilityCharacterState_Walk::UAbilityCharacterState_Walk()
{
	StateName = FName("Walk");
}

void UAbilityCharacterState_Walk::OnInitialize(UFSMComponent* InFSMComponent, int32 InStateIndex)
{
	Super::OnInitialize(InFSMComponent, InStateIndex);
}

bool UAbilityCharacterState_Walk::OnValidate()
{
	return Super::OnValidate();
}

void UAbilityCharacterState_Walk::OnEnter(UFiniteStateBase* InLastFiniteState)
{
	Super::OnEnter(InLastFiniteState);
}

void UAbilityCharacterState_Walk::OnRefresh()
{
	Super::OnRefresh();
}

void UAbilityCharacterState_Walk::OnLeave(UFiniteStateBase* InNextFiniteState)
{
	Super::OnLeave(InNextFiniteState);
}

void UAbilityCharacterState_Walk::OnTermination()
{
	Super::OnTermination();
}

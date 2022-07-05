// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Character/States/AbilityCharacterState_Fall.h"

#include "Ability/Character/AbilityCharacterBase.h"

UAbilityCharacterState_Fall::UAbilityCharacterState_Fall()
{
	StateName = FName("Fall");
}

void UAbilityCharacterState_Fall::OnInitialize(UFSMComponent* InFSMComponent, int32 InStateIndex)
{
	Super::OnInitialize(InFSMComponent, InStateIndex);
}

bool UAbilityCharacterState_Fall::OnValidate()
{
	return Super::OnValidate();
}

void UAbilityCharacterState_Fall::OnEnter(UFiniteStateBase* InLastFiniteState)
{
	Super::OnEnter(InLastFiniteState);
}

void UAbilityCharacterState_Fall::OnRefresh()
{
	Super::OnRefresh();
}

void UAbilityCharacterState_Fall::OnLeave(UFiniteStateBase* InNextFiniteState)
{
	Super::OnLeave(InNextFiniteState);
}

void UAbilityCharacterState_Fall::OnTermination()
{
	Super::OnTermination();
}

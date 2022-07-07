// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Character/States/AbilityCharacterState_Default.h"

#include "Ability/Character/AbilityCharacterBase.h"

UAbilityCharacterState_Default::UAbilityCharacterState_Default()
{
	StateName = FName("Default");
}

void UAbilityCharacterState_Default::OnInitialize(UFSMComponent* InFSMComponent, int32 InStateIndex)
{
	Super::OnInitialize(InFSMComponent, InStateIndex);
}

bool UAbilityCharacterState_Default::OnValidate()
{
	return Super::OnValidate();
}

void UAbilityCharacterState_Default::OnEnter(UFiniteStateBase* InLastFiniteState)
{
	Super::OnEnter(InLastFiniteState);

	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();

	Character->SetVisible(true);
	Character->SetHealth(-1.f);
	Character->SetMotionRate(1, 1);
}

void UAbilityCharacterState_Default::OnRefresh()
{
	Super::OnRefresh();
}

void UAbilityCharacterState_Default::OnLeave(UFiniteStateBase* InNextFiniteState)
{
	Super::OnLeave(InNextFiniteState);
}

void UAbilityCharacterState_Default::OnTermination()
{
	Super::OnTermination();
}

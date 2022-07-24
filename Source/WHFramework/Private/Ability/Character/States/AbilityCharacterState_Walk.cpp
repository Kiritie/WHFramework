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

bool UAbilityCharacterState_Walk::OnEnterValidate(UFiniteStateBase* InLastFiniteState)
{
	if(!Super::OnEnterValidate(InLastFiniteState)) return false;
	
	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();

	return Character->GetCharacterMovement()->IsWalking();
}

void UAbilityCharacterState_Walk::OnEnter(UFiniteStateBase* InLastFiniteState)
{
	Super::OnEnter(InLastFiniteState);
}

void UAbilityCharacterState_Walk::OnRefresh()
{
	Super::OnRefresh();
}

bool UAbilityCharacterState_Walk::OnLeaveValidate(UFiniteStateBase* InNextFiniteState)
{
	if(!Super::OnLeaveValidate(InNextFiniteState)) return false;
	
	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();

	return !Character->GetCharacterMovement()->IsFalling() || InNextFiniteState && InNextFiniteState->IsA<UAbilityCharacterState_Jump>();
}

void UAbilityCharacterState_Walk::OnLeave(UFiniteStateBase* InNextFiniteState)
{
	Super::OnLeave(InNextFiniteState);
}

void UAbilityCharacterState_Walk::OnTermination()
{
	Super::OnTermination();
}

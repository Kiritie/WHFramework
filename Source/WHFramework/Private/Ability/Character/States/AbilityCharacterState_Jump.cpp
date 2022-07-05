// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Character/States/AbilityCharacterState_Jump.h"

#include "Ability/Character/AbilityCharacterBase.h"

UAbilityCharacterState_Jump::UAbilityCharacterState_Jump()
{
	StateName = FName("Jump");
	
	LastFiniteState = nullptr;
}

void UAbilityCharacterState_Jump::OnInitialize(UFSMComponent* InFSMComponent, int32 InStateIndex)
{
	Super::OnInitialize(InFSMComponent, InStateIndex);
}

bool UAbilityCharacterState_Jump::OnValidate()
{
	return Super::OnValidate();
}

void UAbilityCharacterState_Jump::OnEnter(UFiniteStateBase* InLastFiniteState)
{
	Super::OnEnter(InLastFiniteState);

	WaitTime = 0.f;
	
	LastFiniteState = InLastFiniteState;

	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();

	Character->Jump();
}

void UAbilityCharacterState_Jump::OnRefresh()
{
	Super::OnRefresh();

	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();

	WaitTime += GetWorld()->GetDeltaSeconds();
	if(WaitTime >= 0.1f)
	{
		Switch(LastFiniteState);
	}
}

void UAbilityCharacterState_Jump::OnLeave(UFiniteStateBase* InNextFiniteState)
{
	Super::OnLeave(InNextFiniteState);

	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();

	Character->StopJumping();
}

void UAbilityCharacterState_Jump::OnTermination()
{
	Super::OnTermination();
}

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

bool UAbilityCharacterState_Fall::OnEnterValidate(UFiniteStateBase* InLastFiniteState)
{
	if(!Super::OnEnterValidate(InLastFiniteState)) return false;
	
	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();

	return Character->GetCharacterMovement()->IsFalling();
}

void UAbilityCharacterState_Fall::OnEnter(UFiniteStateBase* InLastFiniteState)
{
	Super::OnEnter(InLastFiniteState);

	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();

	Character->GetAbilitySystemComponent()->AddLooseGameplayTag(Character->GetCharacterData<UAbilityCharacterDataBase>().FallingTag);
}

void UAbilityCharacterState_Fall::OnRefresh()
{
	Super::OnRefresh();
}

bool UAbilityCharacterState_Fall::OnLeaveValidate(UFiniteStateBase* InNextFiniteState)
{
	if(!Super::OnLeaveValidate(InNextFiniteState)) return false;
	
	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();

	return !Character->GetCharacterMovement()->IsFalling() || InNextFiniteState && InNextFiniteState->IsA<UAbilityCharacterState_Jump>();
}

void UAbilityCharacterState_Fall::OnLeave(UFiniteStateBase* InNextFiniteState)
{
	Super::OnLeave(InNextFiniteState);

	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();

	Character->GetAbilitySystemComponent()->RemoveLooseGameplayTag(Character->GetCharacterData<UAbilityCharacterDataBase>().FallingTag);
}

void UAbilityCharacterState_Fall::OnTermination()
{
	Super::OnTermination();
}

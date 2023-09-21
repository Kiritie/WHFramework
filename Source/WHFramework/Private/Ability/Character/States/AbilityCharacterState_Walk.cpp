// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Character/States/AbilityCharacterState_Walk.h"

#include "Ability/Character/AbilityCharacterBase.h"
#include "Ability/Character/AbilityCharacterDataBase.h"

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
	return Super::OnEnterValidate(InLastFiniteState);
}

void UAbilityCharacterState_Walk::OnEnter(UFiniteStateBase* InLastFiniteState)
{
	Super::OnEnter(InLastFiniteState);

	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();

	Character->GetAbilitySystemComponent()->AddLooseGameplayTag(Character->GetCharacterData<UAbilityCharacterDataBase>().WalkingTag);
}

void UAbilityCharacterState_Walk::OnRefresh()
{
	Super::OnRefresh();
}

bool UAbilityCharacterState_Walk::OnLeaveValidate(UFiniteStateBase* InNextFiniteState)
{
	return Super::OnLeaveValidate(InNextFiniteState);
}

void UAbilityCharacterState_Walk::OnLeave(UFiniteStateBase* InNextFiniteState)
{
	Super::OnLeave(InNextFiniteState);

	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();

	Character->GetAbilitySystemComponent()->RemoveLooseGameplayTag(Character->GetCharacterData<UAbilityCharacterDataBase>().WalkingTag);
}

void UAbilityCharacterState_Walk::OnTermination()
{
	Super::OnTermination();
}

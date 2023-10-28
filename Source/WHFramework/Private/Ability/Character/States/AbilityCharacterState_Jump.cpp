// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Character/States/AbilityCharacterState_Jump.h"

#include "Ability/Character/AbilityCharacterBase.h"
#include "Ability/Character/AbilityCharacterDataBase.h"

UAbilityCharacterState_Jump::UAbilityCharacterState_Jump()
{
	StateName = FName("Jump");
	
}

void UAbilityCharacterState_Jump::OnInitialize(UFSMComponent* InFSMComponent, int32 InStateIndex)
{
	Super::OnInitialize(InFSMComponent, InStateIndex);
}

bool UAbilityCharacterState_Jump::OnEnterValidate(UFiniteStateBase* InLastFiniteState)
{
	return Super::OnEnterValidate(InLastFiniteState);
}

void UAbilityCharacterState_Jump::OnEnter(UFiniteStateBase* InLastFiniteState)
{
	Super::OnEnter(InLastFiniteState);
	
	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();
	
	Character->GetAbilitySystemComponent()->AddLooseGameplayTag(GameplayTags::StateTag_Character_Jumping);

	Character->Jump();
}

void UAbilityCharacterState_Jump::OnRefresh()
{
	Super::OnRefresh();
}

void UAbilityCharacterState_Jump::OnLeave(UFiniteStateBase* InNextFiniteState)
{
	Super::OnLeave(InNextFiniteState);

	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();
	
	Character->GetAbilitySystemComponent()->RemoveLooseGameplayTag(GameplayTags::StateTag_Character_Jumping);

	Character->StopJumping();
}

void UAbilityCharacterState_Jump::OnTermination()
{
	Super::OnTermination();
}

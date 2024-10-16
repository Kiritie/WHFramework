// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Character/States/AbilityCharacterState_Fall.h"

#include "AbilitySystemComponent.h"
#include "Ability/Character/AbilityCharacterBase.h"

UAbilityCharacterState_Fall::UAbilityCharacterState_Fall()
{
	StateName = FName("Fall");
}

void UAbilityCharacterState_Fall::OnInitialize(UFSMComponent* InFSM, int32 InStateIndex)
{
	Super::OnInitialize(InFSM, InStateIndex);
}

bool UAbilityCharacterState_Fall::OnPreEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	return Super::OnPreEnter(InLastState, InParams);
}

void UAbilityCharacterState_Fall::OnEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	Super::OnEnter(InLastState, InParams);

	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();

	Character->GetAbilitySystemComponent()->AddLooseGameplayTag(GameplayTags::State_Character_Falling);
}

void UAbilityCharacterState_Fall::OnRefresh(float DeltaSeconds)
{
	Super::OnRefresh(DeltaSeconds);
}

bool UAbilityCharacterState_Fall::OnPreLeave(UFiniteStateBase* InNextState)
{
	return Super::OnPreLeave(InNextState);
}

void UAbilityCharacterState_Fall::OnLeave(UFiniteStateBase* InNextState)
{
	Super::OnLeave(InNextState);

	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();

	Character->GetAbilitySystemComponent()->RemoveLooseGameplayTag(GameplayTags::State_Character_Falling);
}

void UAbilityCharacterState_Fall::OnTermination()
{
	Super::OnTermination();
}

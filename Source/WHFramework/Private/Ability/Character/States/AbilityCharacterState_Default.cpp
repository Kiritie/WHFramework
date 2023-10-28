// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Character/States/AbilityCharacterState_Default.h"

#include "Ability/Character/AbilityCharacterBase.h"
#include "Ability/Character/AbilityCharacterDataBase.h"
#include "FSM/Components/FSMComponent.h"

UAbilityCharacterState_Default::UAbilityCharacterState_Default()
{
	StateName = FName("Default");
}

void UAbilityCharacterState_Default::OnInitialize(UFSMComponent* InFSMComponent, int32 InStateIndex)
{
	Super::OnInitialize(InFSMComponent, InStateIndex);
}

bool UAbilityCharacterState_Default::OnEnterValidate(UFiniteStateBase* InLastFiniteState)
{
	return Super::OnEnterValidate(InLastFiniteState);
}

void UAbilityCharacterState_Default::OnEnter(UFiniteStateBase* InLastFiniteState)
{
	Super::OnEnter(InLastFiniteState);

	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();

	Character->GetAbilitySystemComponent()->RemoveLooseGameplayTag(GameplayTags::StateTag_Character_Active);

	Character->Execute_SetActorVisible(Character, true);
	Character->ResetData();
	Character->SetMotionRate(1, 1);
}

void UAbilityCharacterState_Default::OnRefresh()
{
	Super::OnRefresh();

	TrySwitchToWalk();
}

void UAbilityCharacterState_Default::OnLeave(UFiniteStateBase* InNextFiniteState)
{
	Super::OnLeave(InNextFiniteState);

	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();

	Character->GetAbilitySystemComponent()->AddLooseGameplayTag(GameplayTags::StateTag_Character_Active);
}

void UAbilityCharacterState_Default::OnTermination()
{
	Super::OnTermination();
}

void UAbilityCharacterState_Default::TrySwitchToWalk()
{
	FSM->SwitchStateByClass<UAbilityCharacterState_Walk>();
}

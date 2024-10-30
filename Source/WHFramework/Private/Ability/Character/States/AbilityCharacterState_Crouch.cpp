// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Character/States/AbilityCharacterState_Crouch.h"

#include "Ability/Character/AbilityCharacterBase.h"

UAbilityCharacterState_Crouch::UAbilityCharacterState_Crouch()
{
	StateName = FName("Crouch");
}

void UAbilityCharacterState_Crouch::OnInitialize(UFSMComponent* InFSM, int32 InStateIndex)
{
	Super::OnInitialize(InFSM, InStateIndex);
}

bool UAbilityCharacterState_Crouch::OnPreEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	if(!Super::OnPreEnter(InLastState, InParams)) return false;

	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();

	return Character->DoAction(GameplayTags::Ability_Character_Action_Crouch);
}

void UAbilityCharacterState_Crouch::OnEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	Super::OnEnter(InLastState, InParams);

	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();
	
	Character->GetAbilitySystemComponent()->AddLooseGameplayTag(GameplayTags::State_Character_Crouching);

	Character->Crouch(InParams[0]);
}

void UAbilityCharacterState_Crouch::OnRefresh(float DeltaSeconds)
{
	Super::OnRefresh(DeltaSeconds);
}

void UAbilityCharacterState_Crouch::OnLeave(UFiniteStateBase* InNextState)
{
	Super::OnLeave(InNextState);

	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();

	Character->StopAction(GameplayTags::Ability_Character_Action_Crouch);
	
	Character->GetAbilitySystemComponent()->RemoveLooseGameplayTag(GameplayTags::State_Character_Crouching);

	Character->FreeToAnim();
}

void UAbilityCharacterState_Crouch::OnTermination()
{
	Super::OnTermination();
}

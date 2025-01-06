// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Character/States/AbilityCharacterState_Climb.h"

#include "Ability/Character/AbilityCharacterBase.h"

UAbilityCharacterState_Climb::UAbilityCharacterState_Climb()
{
	StateName = FName("Climb");
}

void UAbilityCharacterState_Climb::OnInitialize(UFSMComponent* InFSM, int32 InStateIndex)
{
	Super::OnInitialize(InFSM, InStateIndex);
}

bool UAbilityCharacterState_Climb::OnPreEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	if(!Super::OnPreEnter(InLastState, InParams)) return false;

	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();

	return Character->DoAction(GameplayTags::Ability_Character_Action_Climb);
}

void UAbilityCharacterState_Climb::OnEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	Super::OnEnter(InLastState, InParams);

	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();
	
	Character->GetAbilitySystemComponent()->AddLooseGameplayTag(GameplayTags::State_Character_Climbing);

	Character->LimitToAnim();
}

void UAbilityCharacterState_Climb::OnRefresh(float DeltaSeconds)
{
	Super::OnRefresh(DeltaSeconds);
}

void UAbilityCharacterState_Climb::OnLeave(UFiniteStateBase* InNextState)
{
	Super::OnLeave(InNextState);

	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();

	Character->StopAction(GameplayTags::Ability_Character_Action_Climb);
		
	Character->GetAbilitySystemComponent()->RemoveLooseGameplayTag(GameplayTags::State_Character_Climbing);

	Character->FreeToAnim();
}

void UAbilityCharacterState_Climb::OnTermination()
{
	Super::OnTermination();
}

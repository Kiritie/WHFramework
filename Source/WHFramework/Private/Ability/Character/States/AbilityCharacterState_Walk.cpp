// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Character/States/AbilityCharacterState_Walk.h"

#include "Ability/Character/AbilityCharacterBase.h"
#include "Common/Interaction/InteractionComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Scene/SceneModuleStatics.h"

UAbilityCharacterState_Walk::UAbilityCharacterState_Walk()
{
	StateName = FName("Walk");
}

void UAbilityCharacterState_Walk::OnInitialize(UFSMComponent* InFSM, int32 InStateIndex)
{
	Super::OnInitialize(InFSM, InStateIndex);
}

bool UAbilityCharacterState_Walk::OnPreEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	return Super::OnPreEnter(InLastState, InParams);
}

void UAbilityCharacterState_Walk::OnEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	Super::OnEnter(InLastState, InParams);

	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();

	Character->GetAbilitySystemComponent()->AddLooseGameplayTag(GameplayTags::StateTag_Character_Walking);
	
	Character->GetInteractionComponent()->SetInteractable(true);

	if(Character->GetCharacterMovement()->MovementMode != MOVE_Walking)
	{
		Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	}
}

void UAbilityCharacterState_Walk::OnRefresh(float DeltaSeconds)
{
	Super::OnRefresh(DeltaSeconds);
}

bool UAbilityCharacterState_Walk::OnPreLeave(UFiniteStateBase* InNextState)
{
	return Super::OnPreLeave(InNextState);
}

void UAbilityCharacterState_Walk::OnLeave(UFiniteStateBase* InNextState)
{
	Super::OnLeave(InNextState);

	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();

	Character->GetAbilitySystemComponent()->RemoveLooseGameplayTag(GameplayTags::StateTag_Character_Walking);
}

void UAbilityCharacterState_Walk::OnTermination()
{
	Super::OnTermination();
}

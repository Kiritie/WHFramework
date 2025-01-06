// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Character/States/AbilityCharacterState_Swim.h"

#include "Ability/Character/AbilityCharacterBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Scene/SceneModuleStatics.h"
#include "Ability/Character/States/AbilityCharacterState_Float.h"

UAbilityCharacterState_Swim::UAbilityCharacterState_Swim()
{
	StateName = FName("Swim");
}

void UAbilityCharacterState_Swim::OnInitialize(UFSMComponent* InFSM, int32 InStateIndex)
{
	Super::OnInitialize(InFSM, InStateIndex);
}

bool UAbilityCharacterState_Swim::OnPreEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	if(!Super::OnPreEnter(InLastState, InParams)) return false;

	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();

	return Character->DoAction(GameplayTags::Ability_Character_Action_Swim);
}

void UAbilityCharacterState_Swim::OnEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	Super::OnEnter(InLastState, InParams);

	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();
	
	Character->GetAbilitySystemComponent()->AddLooseGameplayTag(GameplayTags::State_Character_Swimming);

	// Character->LimitToAnim();

	if(Character->GetCharacterMovement()->MovementMode != MOVE_Swimming)
	{
		Character->GetCharacterMovement()->SetMovementMode(MOVE_Swimming);
	}

	Character->GetMovementComponent()->Velocity.Z *= 0.2f;
}

void UAbilityCharacterState_Swim::OnRefresh(float DeltaSeconds)
{
	Super::OnRefresh(DeltaSeconds);
}

void UAbilityCharacterState_Swim::OnLeave(UFiniteStateBase* InNextState)
{
	Super::OnLeave(InNextState);

	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();

	Character->StopAction(GameplayTags::Ability_Character_Action_Swim);
	
	Character->GetAbilitySystemComponent()->RemoveLooseGameplayTag(GameplayTags::State_Character_Swimming);
}

void UAbilityCharacterState_Swim::OnTermination()
{
	Super::OnTermination();
}

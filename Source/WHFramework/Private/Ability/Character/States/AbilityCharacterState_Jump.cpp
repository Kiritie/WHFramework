// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Character/States/AbilityCharacterState_Jump.h"

#include "Ability/Character/AbilityCharacterBase.h"
#include "Ability/Character/States/AbilityCharacterState_Fall.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Scene/SceneModuleStatics.h"

UAbilityCharacterState_Jump::UAbilityCharacterState_Jump()
{
	StateName = FName("Jump");
}

void UAbilityCharacterState_Jump::OnInitialize(UFSMComponent* InFSM, int32 InStateIndex)
{
	Super::OnInitialize(InFSM, InStateIndex);
}

bool UAbilityCharacterState_Jump::OnPreEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	return Super::OnPreEnter(InLastState, InParams);
}

void UAbilityCharacterState_Jump::OnEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	Super::OnEnter(InLastState, InParams);
	
	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();
	
	Character->GetAbilitySystemComponent()->AddLooseGameplayTag(GameplayTags::State_Character_Jumping);

	if(Character->GetCharacterMovement()->MovementMode != MOVE_Walking)
	{
		Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		if(Character->GetCharacterMovement()->UpdatedComponent)
		{
			Character->GetCharacterMovement()->UpdatedComponent->SetPhysicsVolume(USceneModuleStatics::GetDefaultPhysicsVolume(), true);
		}
	}

	Character->Jump();
}

void UAbilityCharacterState_Jump::OnRefresh(float DeltaSeconds)
{
	Super::OnRefresh(DeltaSeconds);
}

void UAbilityCharacterState_Jump::OnLeave(UFiniteStateBase* InNextState)
{
	Super::OnLeave(InNextState);

	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();
	
	Character->GetAbilitySystemComponent()->RemoveLooseGameplayTag(GameplayTags::State_Character_Jumping);

	Character->StopJumping();
}

void UAbilityCharacterState_Jump::OnTermination()
{
	Super::OnTermination();
}

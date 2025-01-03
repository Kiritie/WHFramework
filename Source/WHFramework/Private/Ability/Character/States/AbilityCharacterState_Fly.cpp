// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Character/States/AbilityCharacterState_Fly.h"

#include "Ability/Character/States/AbilityCharacterState_Walk.h"
#include "Ability/Character/AbilityCharacterBase.h"
#include "FSM/Components/FSMComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Scene/SceneModuleStatics.h"

UAbilityCharacterState_Fly::UAbilityCharacterState_Fly()
{
	StateName = FName("Fly");
}

void UAbilityCharacterState_Fly::OnInitialize(UFSMComponent* InFSM, int32 InStateIndex)
{
	Super::OnInitialize(InFSM, InStateIndex);
}

bool UAbilityCharacterState_Fly::OnPreEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	if(!Super::OnPreEnter(InLastState, InParams)) return false;

	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();

	return Character->DoAction(GameplayTags::Ability_Character_Action_Fly);
}

void UAbilityCharacterState_Fly::OnEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	Super::OnEnter(InLastState, InParams);

	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();
	
	Character->GetAbilitySystemComponent()->AddLooseGameplayTag(GameplayTags::State_Character_Flying);

	Character->LimitToAnim();
	Character->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
	Character->GetCharacterMovement()->Velocity *= 0.2f;
	// Character->GetCharacterMovement()->GravityScale = 0.f;
	// Character->GetCharacterMovement()->AirControl = 1.f;

	if(Character->GetCharacterMovement()->MovementMode != MOVE_Flying)
	{
		Character->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
	}
}

void UAbilityCharacterState_Fly::OnRefresh(float DeltaSeconds)
{
	Super::OnRefresh(DeltaSeconds);

	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();

	if(Character->GetVelocity().Z < 0.f)
	{
		FFindFloorResult FindFloorResult;
		Character->GetCharacterMovement()->FindFloor(Character->GetCharacterMovement()->UpdatedComponent->GetComponentLocation(), FindFloorResult, Character->GetVelocity().IsZero(), nullptr);
		if(FindFloorResult.IsWalkableFloor())
		{
			FSM->SwitchStateByClass<UAbilityCharacterState_Walk>();
		}
	}
}

void UAbilityCharacterState_Fly::OnLeave(UFiniteStateBase* InNextState)
{
	Super::OnLeave(InNextState);

	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();

	Character->StopAction(GameplayTags::Ability_Character_Action_Fly);
	
	Character->GetAbilitySystemComponent()->RemoveLooseGameplayTag(GameplayTags::State_Character_Flying);

	Character->FreeToAnim();
	Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	// Character->GetCharacterMovement()->GravityScale = DefaultGravityScale;
	// Character->GetCharacterMovement()->AirControl = DefaultAirControl;
}

void UAbilityCharacterState_Fly::OnTermination()
{
	Super::OnTermination();
}

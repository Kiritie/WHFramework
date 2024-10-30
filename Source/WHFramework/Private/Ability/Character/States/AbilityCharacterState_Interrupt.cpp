// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Character/States/AbilityCharacterState_Interrupt.h"

#include "Ability/Character/AbilityCharacterBase.h"
#include "Ability/Character/States/AbilityCharacterState_Walk.h"
#include "FSM/Components/FSMComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

UAbilityCharacterState_Interrupt::UAbilityCharacterState_Interrupt()
{
	StateName = FName("Interrupt");

	Duration = 0.f;
	RemainTime = 0.f;
}

void UAbilityCharacterState_Interrupt::OnInitialize(UFSMComponent* InFSM, int32 InStateIndex)
{
	Super::OnInitialize(InFSM, InStateIndex);
}

bool UAbilityCharacterState_Interrupt::OnPreEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	if(!Super::OnPreEnter(InLastState, InParams)) return false;

	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();

	return Character->DoAction(GameplayTags::Ability_Pawn_Action_Interrupt);
}

void UAbilityCharacterState_Interrupt::OnEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	Super::OnEnter(InLastState, InParams);

	if(InParams.IsValidIndex(0))
	{
		Duration = InParams[0];
		RemainTime = Duration;
	}

	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();
	
	Character->GetAbilitySystemComponent()->AddLooseGameplayTag(GameplayTags::State_Vitality_Interrupting);

	Character->GetCharacterMovement()->SetActive(false);

	Character->LimitToAnim();
}

void UAbilityCharacterState_Interrupt::OnRefresh(float DeltaSeconds)
{
	Super::OnRefresh(DeltaSeconds);

	RemainTime -= GetWorld()->GetDeltaSeconds();
	if (RemainTime <= 0.f)
	{
		FSM->RefreshState();
	}
}

void UAbilityCharacterState_Interrupt::OnLeave(UFiniteStateBase* InNextState)
{
	Super::OnLeave(InNextState);

	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();

	Character->StopAction(GameplayTags::Ability_Pawn_Action_Interrupt);
		
	Character->GetAbilitySystemComponent()->RemoveLooseGameplayTag(GameplayTags::State_Vitality_Interrupting);

	Character->GetCharacterMovement()->SetActive(true);

	Character->FreeToAnim();
}

void UAbilityCharacterState_Interrupt::OnTermination()
{
	Super::OnTermination();
}

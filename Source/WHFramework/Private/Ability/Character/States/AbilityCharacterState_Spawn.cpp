// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Character/States/AbilityCharacterState_Spawn.h"

#include "Ability/Character/AbilityCharacterBase.h"
#include "Common/Interaction/InteractionComponent.h"
#include "FSM/Components/FSMComponent.h"

UAbilityCharacterState_Spawn::UAbilityCharacterState_Spawn()
{
	StateName = FName("Default");
}

void UAbilityCharacterState_Spawn::OnInitialize(UFSMComponent* InFSM, int32 InStateIndex)
{
	Super::OnInitialize(InFSM, InStateIndex);
}

bool UAbilityCharacterState_Spawn::OnPreEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	return Super::OnPreEnter(InLastState, InParams);
}

void UAbilityCharacterState_Spawn::OnEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	Super::OnEnter(InLastState, InParams);

	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();

	Character->GetAbilitySystemComponent()->RemoveLooseGameplayTag(GameplayTags::StateTag_Character_Active);

	Character->GetInteractionComponent()->SetInteractable(false);

	Character->Execute_SetActorVisible(Character, true);
	Character->ResetData();
	Character->SetMotionRate(1, 1);
}

void UAbilityCharacterState_Spawn::OnRefresh(float DeltaSeconds)
{
	Super::OnRefresh(DeltaSeconds);

	TryLeave();
}

void UAbilityCharacterState_Spawn::OnLeave(UFiniteStateBase* InNextState)
{
	Super::OnLeave(InNextState);

	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();

	Character->GetAbilitySystemComponent()->AddLooseGameplayTag(GameplayTags::StateTag_Character_Active);
}

void UAbilityCharacterState_Spawn::OnTermination()
{
	Super::OnTermination();
}

void UAbilityCharacterState_Spawn::TryLeave()
{
	FSM->SwitchStateByClass<UAbilityCharacterState_Walk>();
}

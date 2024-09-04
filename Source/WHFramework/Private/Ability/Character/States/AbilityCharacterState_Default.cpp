// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Character/States/AbilityCharacterState_Default.h"

#include "Ability/Character/AbilityCharacterBase.h"
#include "Common/Interaction/InteractionComponent.h"
#include "FSM/Components/FSMComponent.h"

UAbilityCharacterState_Default::UAbilityCharacterState_Default()
{
	StateName = FName("Default");
}

void UAbilityCharacterState_Default::OnInitialize(UFSMComponent* InFSM, int32 InStateIndex)
{
	Super::OnInitialize(InFSM, InStateIndex);
}

bool UAbilityCharacterState_Default::OnEnterValidate(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	return Super::OnEnterValidate(InLastState, InParams);
}

void UAbilityCharacterState_Default::OnEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	Super::OnEnter(InLastState, InParams);

	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();

	Character->GetAbilitySystemComponent()->RemoveLooseGameplayTag(GameplayTags::StateTag_Character_Active);

	Character->GetInteractionComponent()->SetInteractable(false);

	Character->Execute_SetActorVisible(Character, true);
	Character->ResetData();
	Character->SetMotionRate(1, 1);
}

void UAbilityCharacterState_Default::OnRefresh(float DeltaSeconds)
{
	Super::OnRefresh(DeltaSeconds);

	TrySwitchToWalk();
}

void UAbilityCharacterState_Default::OnLeave(UFiniteStateBase* InNextState)
{
	Super::OnLeave(InNextState);

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

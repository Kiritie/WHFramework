// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Vitality/States/AbilityVitalityState_Spawn.h"

#include "Ability/Vitality/AbilityVitalityBase.h"
#include "Ability/Vitality/States/AbilityVitalityState_Walk.h"
#include "Common/Interaction/InteractionComponent.h"
#include "Components/ShapeComponent.h"
#include "FSM/Components/FSMComponent.h"

UAbilityVitalityState_Spawn::UAbilityVitalityState_Spawn()
{
	StateName = FName("Spawn");
}

void UAbilityVitalityState_Spawn::OnInitialize(UFSMComponent* InFSM, int32 InStateIndex)
{
	Super::OnInitialize(InFSM, InStateIndex);
}

bool UAbilityVitalityState_Spawn::OnPreEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	return Super::OnPreEnter(InLastState, InParams);
}

void UAbilityVitalityState_Spawn::OnEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	Super::OnEnter(InLastState, InParams);

	AAbilityVitalityBase* Vitality = GetAgent<AAbilityVitalityBase>();

	Vitality->GetAbilitySystemComponent()->RemoveLooseGameplayTag(GameplayTags::State_Vitality_Active);

	Vitality->DoAction(GameplayTags::Ability_Vitality_Action_Spawn);

	Vitality->GetCollisionComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Vitality->GetInteractionComponent()->SetInteractable(false);
	
	Vitality->ResetData();
}

void UAbilityVitalityState_Spawn::OnRefresh(float DeltaSeconds)
{
	Super::OnRefresh(DeltaSeconds);

	TryLeave();
}

void UAbilityVitalityState_Spawn::OnLeave(UFiniteStateBase* InNextState)
{
	Super::OnLeave(InNextState);

	AAbilityVitalityBase* Vitality = GetAgent<AAbilityVitalityBase>();

	Vitality->GetAbilitySystemComponent()->AddLooseGameplayTag(GameplayTags::State_Vitality_Active);
	
	Vitality->StopAction(GameplayTags::Ability_Vitality_Action_Spawn);

	Vitality->GetCollisionComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Vitality->GetInteractionComponent()->SetInteractable(true);
}

void UAbilityVitalityState_Spawn::OnTermination()
{
	Super::OnTermination();
}

void UAbilityVitalityState_Spawn::TryLeave()
{
	FSM->SwitchStateByClass<UAbilityVitalityState_Walk>();
}

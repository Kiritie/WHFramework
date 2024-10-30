// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Pawn/States/AbilityPawnState_Spawn.h"

#include "Ability/Pawn/AbilityPawnBase.h"
#include "Ability/Pawn/States/AbilityPawnState_Walk.h"
#include "AI/Base/AIControllerBase.h"
#include "Common/Interaction/InteractionComponent.h"
#include "Components/ShapeComponent.h"
#include "FSM/Components/FSMComponent.h"

UAbilityPawnState_Spawn::UAbilityPawnState_Spawn()
{
	StateName = FName("Spawn");
}

void UAbilityPawnState_Spawn::OnInitialize(UFSMComponent* InFSM, int32 InStateIndex)
{
	Super::OnInitialize(InFSM, InStateIndex);
}

bool UAbilityPawnState_Spawn::OnPreEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	return Super::OnPreEnter(InLastState, InParams);
}

void UAbilityPawnState_Spawn::OnEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	Super::OnEnter(InLastState, InParams);

	AAbilityPawnBase* Pawn = GetAgent<AAbilityPawnBase>();

	Pawn->GetAbilitySystemComponent()->RemoveLooseGameplayTag(GameplayTags::State_Vitality_Active);

	Pawn->DoAction(GameplayTags::Ability_Pawn_Action_Spawn);

	Pawn->ResetData();

	Pawn->GetCollisionComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Pawn->GetInteractionComponent()->SetInteractable(false);
}

void UAbilityPawnState_Spawn::OnRefresh(float DeltaSeconds)
{
	Super::OnRefresh(DeltaSeconds);

	TryLeave();
}

void UAbilityPawnState_Spawn::OnLeave(UFiniteStateBase* InNextState)
{
	Super::OnLeave(InNextState);

	AAbilityPawnBase* Pawn = GetAgent<AAbilityPawnBase>();

	Pawn->GetAbilitySystemComponent()->AddLooseGameplayTag(GameplayTags::State_Vitality_Active);
	
	Pawn->StopAction(GameplayTags::Ability_Pawn_Action_Spawn);

	Pawn->GetCollisionComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Pawn->GetInteractionComponent()->SetInteractable(true);

	if(!Pawn->IsPlayer())
	{
		Pawn->GetController<AAIControllerBase>()->RunBehaviorTree();
	}
}

void UAbilityPawnState_Spawn::OnTermination()
{
	Super::OnTermination();
}

void UAbilityPawnState_Spawn::TryLeave()
{
	FSM->SwitchStateByClass<UAbilityPawnState_Walk>();
}

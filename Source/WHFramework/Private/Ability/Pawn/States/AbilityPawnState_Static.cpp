// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Pawn/States/AbilityPawnState_Static.h"

#include "AbilitySystemComponent.h"
#include "Ability/Pawn/AbilityPawnBase.h"
#include "AI/Base/AIControllerBase.h"
#include "Common/Interaction/InteractionComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/PawnMovementComponent.h"

UAbilityPawnState_Static::UAbilityPawnState_Static()
{
	StateName = FName("Static");
}

void UAbilityPawnState_Static::OnInitialize(UFSMComponent* InFSM, int32 InStateIndex)
{
	Super::OnInitialize(InFSM, InStateIndex);
}

bool UAbilityPawnState_Static::OnPreEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	return Super::OnPreEnter(InLastState, InParams);
}

void UAbilityPawnState_Static::OnEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	Super::OnEnter(InLastState, InParams);

	AAbilityPawnBase* Pawn = GetAgent<AAbilityPawnBase>();

	Pawn->GetAbilitySystemComponent()->RemoveLooseGameplayTag(GameplayTags::State_Vitality_Active);

	Pawn->DoAction(GameplayTags::Ability_Vitality_Action_Static);

	Pawn->GetMovementComponent()->SetActive(false);
	Pawn->GetCollisionComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Pawn->GetInteractionComponent()->SetInteractable(false);

	if(!Pawn->IsPlayer())
	{
		Pawn->GetController<AAIControllerBase>()->StopBehaviorTree();
	}
}

void UAbilityPawnState_Static::OnRefresh(float DeltaSeconds)
{
	Super::OnRefresh(DeltaSeconds);
}

void UAbilityPawnState_Static::OnLeave(UFiniteStateBase* InNextState)
{
	Super::OnLeave(InNextState);

	AAbilityPawnBase* Pawn = GetAgent<AAbilityPawnBase>();

	Pawn->GetAbilitySystemComponent()->AddLooseGameplayTag(GameplayTags::State_Vitality_Active);

	Pawn->StopAction(GameplayTags::Ability_Vitality_Action_Static);

	Pawn->GetMovementComponent()->SetActive(true);
	Pawn->GetCollisionComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Pawn->GetInteractionComponent()->SetInteractable(true);

	if(!Pawn->IsPlayer())
	{
		Pawn->GetController<AAIControllerBase>()->RunBehaviorTree();
	}
}

void UAbilityPawnState_Static::OnTermination()
{
	Super::OnTermination();
}

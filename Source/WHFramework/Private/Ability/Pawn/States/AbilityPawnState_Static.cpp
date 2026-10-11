// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Pawn/States/AbilityPawnState_Static.h"
#include "Ability/AbilityActionTypes.h"
#include "FSM/Components/FSMComponent.h"

#include "AbilitySystemComponent.h"
#include "Ability/Pawn/AbilityPawnBase.h"
#include "AI/Base/AIControllerBase.h"
#include "Common/Interaction/InteractionComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/PawnMovementComponent.h"

UAbilityPawnState_Static::UAbilityPawnState_Static()
{
	StateName = FName("Static");
	StateDomain = TEXT("Control");
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

	SetStateTag(AbilityActionTags::StaticControl, true);


	if (Pawn->GetMovementComponent()) { Pawn->GetMovementComponent()->StopMovementImmediately(); Pawn->GetMovementComponent()->SetActive(false); }

	if (AAIControllerBase* Controller = Pawn->GetController<AAIControllerBase>())
	{
		Controller->StopBehaviorTree();
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

	SetStateTag(AbilityActionTags::StaticControl, false);


	if (Pawn->GetMovementComponent() && FSM && !FSM->IsTerminating() && !InNextState && Pawn->IsActive() && !Pawn->IsDead()) { Pawn->GetMovementComponent()->SetActive(true); }

	if (AAIControllerBase* Controller = Pawn->GetController<AAIControllerBase>())
	{
		if (FSM && !FSM->IsTerminating() && !InNextState && Pawn->IsActive() && !Pawn->IsDead()) { Controller->RunBehaviorTree(); }
	}
}

void UAbilityPawnState_Static::OnTermination()
{
	Super::OnTermination();
}

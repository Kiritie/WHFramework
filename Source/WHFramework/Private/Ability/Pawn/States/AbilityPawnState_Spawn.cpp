// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Pawn/States/AbilityPawnState_Spawn.h"

#include "Ability/Pawn/AbilityPawnBase.h"
#include "Ability/Pawn/States/AbilityPawnState_Walk.h"
#include "AI/Base/AIControllerBase.h"
#include "Common/Interaction/InteractionComponent.h"
#include "Components/ShapeComponent.h"
#include "Event/EventModuleStatics.h"
#include "Event/Events/Ability/Event_VitalitySpawned.h"
#include "FSM/Components/FSMComponent.h"
#include "GameFramework/PawnMovementComponent.h"

UAbilityPawnState_Spawn::UAbilityPawnState_Spawn()
{
	StateName = FName("Spawn");

	RescuerActor.Reset();
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

	if(InParams.IsValidIndex(0))
	{
		RescuerActor = Cast<AActor>(InParams[0].Get<UObject*>());
	}

	UEventModuleStatics::BroadcastEvent<FEventVitalitySpawned>(this, { GetAgent(), RescuerActor.Get() });

	AAbilityPawnBase* Pawn = GetAgent<AAbilityPawnBase>();

	SetStateTag(GameplayTags::State_Vitality_Active, false);


	if (Pawn->GetMovementComponent()) { Pawn->GetMovementComponent()->SetActive(false); }
	Pawn->GetCollisionComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Pawn->GetInteractionComponent()->SetInteractable(false);

	Pawn->ResetData();
	Pawn->SetMotionRate(1.f, 1.f);
}

void UAbilityPawnState_Spawn::OnRefresh(float DeltaSeconds)
{
	Super::OnRefresh(DeltaSeconds);

	TryLeave();
}

void UAbilityPawnState_Spawn::OnLeave(UFiniteStateBase* InNextState)
{
	Super::OnLeave(InNextState);
	RescuerActor.Reset();
	AAbilityPawnBase* Pawn = GetAgent<AAbilityPawnBase>();
	if (!Pawn || !FSM || FSM->IsTerminating() || !InNextState || !InNextState->IsA<UAbilityPawnState_Walk>())
	{
		return;
	}
	Pawn->GetCollisionComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Pawn->GetInteractionComponent()->SetInteractable(true);
	if (Pawn->GetMovementComponent())
	{
		if (Pawn->GetMovementComponent()) { Pawn->GetMovementComponent()->SetActive(true); }
	}
	if (AAIControllerBase* Controller = Pawn->GetController<AAIControllerBase>())
	{
		Controller->RunBehaviorTree();
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

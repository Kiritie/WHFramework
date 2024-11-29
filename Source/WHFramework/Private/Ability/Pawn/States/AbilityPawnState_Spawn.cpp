// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Pawn/States/AbilityPawnState_Spawn.h"

#include "Ability/Pawn/AbilityPawnBase.h"
#include "Ability/Pawn/States/AbilityPawnState_Walk.h"
#include "AI/Base/AIControllerBase.h"
#include "Common/Interaction/InteractionComponent.h"
#include "Components/ShapeComponent.h"
#include "Event/EventModuleStatics.h"
#include "Event/Handle/Ability/EventHandle_VitalitySpawned.h"
#include "FSM/Components/FSMComponent.h"
#include "GameFramework/PawnMovementComponent.h"

UAbilityPawnState_Spawn::UAbilityPawnState_Spawn()
{
	StateName = FName("Spawn");

	Rescuer = nullptr;
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
		Rescuer = InParams[0].GetPointerValue<IAbilityVitalityInterface>();
	}
	
	UEventModuleStatics::BroadcastEvent<UEventHandle_VitalitySpawned>(Cast<UObject>(this), { GetAgent(), Cast<UObject>(Rescuer) });

	AAbilityPawnBase* Pawn = GetAgent<AAbilityPawnBase>();

	Pawn->GetAbilitySystemComponent()->RemoveLooseGameplayTag(GameplayTags::State_Vitality_Active);

	Pawn->DoAction(GameplayTags::Ability_Vitality_Action_Spawn);

	Pawn->GetMovementComponent()->SetActive(false);
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

	Rescuer = nullptr;

	AAbilityPawnBase* Pawn = GetAgent<AAbilityPawnBase>();

	Pawn->GetAbilitySystemComponent()->AddLooseGameplayTag(GameplayTags::State_Vitality_Active);
	
	Pawn->StopAction(GameplayTags::Ability_Vitality_Action_Spawn);

	Pawn->GetMovementComponent()->SetActive(true);
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

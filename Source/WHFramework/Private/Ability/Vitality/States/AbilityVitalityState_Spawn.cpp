// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Vitality/States/AbilityVitalityState_Spawn.h"

#include "Ability/Vitality/AbilityVitalityBase.h"
#include "Ability/Vitality/States/AbilityVitalityState_Walk.h"
#include "Common/Interaction/InteractionComponent.h"
#include "Components/ShapeComponent.h"
#include "Event/EventModuleStatics.h"
#include "Event/Events/Ability/Event_VitalitySpawned.h"
#include "FSM/Components/FSMComponent.h"

UAbilityVitalityState_Spawn::UAbilityVitalityState_Spawn()
{
	StateName = FName("Spawn");

	RescuerActor.Reset();
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

	if(InParams.IsValidIndex(0))
	{
		RescuerActor = Cast<AActor>(InParams[0].Get<UObject*>());
	}

	UEventModuleStatics::BroadcastEvent<FEventVitalitySpawned>(this, { GetAgent(), RescuerActor.Get() });

	AAbilityVitalityBase* Vitality = GetAgent<AAbilityVitalityBase>();

	SetStateTag(GameplayTags::State_Vitality_Active, false);


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
	RescuerActor.Reset();
	AAbilityVitalityBase* Vitality = GetAgent<AAbilityVitalityBase>();
	if (!Vitality || !FSM || FSM->IsTerminating() || !InNextState || !InNextState->IsA<UAbilityVitalityState_Walk>())
	{
		return;
	}
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

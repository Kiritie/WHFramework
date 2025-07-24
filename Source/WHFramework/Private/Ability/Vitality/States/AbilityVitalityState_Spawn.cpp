// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Vitality/States/AbilityVitalityState_Spawn.h"

#include "Ability/Vitality/AbilityVitalityBase.h"
#include "Ability/Vitality/States/AbilityVitalityState_Walk.h"
#include "Common/Interaction/InteractionComponent.h"
#include "Components/ShapeComponent.h"
#include "Event/EventModuleStatics.h"
#include "Event/Handle/Ability/EventHandle_VitalitySpawned.h"
#include "FSM/Components/FSMComponent.h"

UAbilityVitalityState_Spawn::UAbilityVitalityState_Spawn()
{
	StateName = FName("Spawn");

	Rescuer = nullptr;
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
		Rescuer = InParams[0].GetPointerValue<IAbilityVitalityInterface>();
	}
	
	UEventModuleStatics::BroadcastEvent<UEventHandle_VitalitySpawned>(this, { GetAgent(), Cast<UObject>(Rescuer) });

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

	Rescuer = nullptr;

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

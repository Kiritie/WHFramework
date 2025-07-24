// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Character/States/AbilityCharacterState_Spawn.h"

#include "Ability/Character/AbilityCharacterBase.h"
#include "AI/Base/AIControllerBase.h"
#include "Common/Interaction/InteractionComponent.h"
#include "Common/Looking/LookingComponent.h"
#include "Components/CapsuleComponent.h"
#include "Event/EventModuleStatics.h"
#include "Event/Handle/Ability/EventHandle_VitalitySpawned.h"
#include "FSM/Components/FSMComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

UAbilityCharacterState_Spawn::UAbilityCharacterState_Spawn()
{
	StateName = FName("Spawn");

	Rescuer = nullptr;
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

	if(InParams.IsValidIndex(0))
	{
		Rescuer = InParams[0].GetPointerValue<IAbilityVitalityInterface>();
	}
	
	UEventModuleStatics::BroadcastEvent<UEventHandle_VitalitySpawned>(this, { GetAgent(), Cast<UObject>(Rescuer) });

	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();

	Character->GetAbilitySystemComponent()->RemoveLooseGameplayTag(GameplayTags::State_Vitality_Active);

	Character->DoAction(GameplayTags::Ability_Vitality_Action_Spawn);

	Character->LimitToAnim();

	Character->GetCharacterMovement()->SetActive(false);
	Character->GetCollisionComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Character->GetInteractionComponent()->SetInteractable(false);

	Character->ResetData();
	Character->SetMotionRate(1.f, 1.f);

	Character->GetLooking()->TargetLookingOff();
}

void UAbilityCharacterState_Spawn::OnRefresh(float DeltaSeconds)
{
	Super::OnRefresh(DeltaSeconds);

	TryLeave();
}

void UAbilityCharacterState_Spawn::OnLeave(UFiniteStateBase* InNextState)
{
	Super::OnLeave(InNextState);

	Rescuer = nullptr;

	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();

	Character->GetAbilitySystemComponent()->AddLooseGameplayTag(GameplayTags::State_Vitality_Active);

	Character->FreeToAnim();

	Character->GetCharacterMovement()->SetActive(true);
	Character->GetCollisionComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Character->GetInteractionComponent()->SetInteractable(true);

	if(Character->GetController<AAIControllerBase>())
	{
		Character->GetController<AAIControllerBase>()->RunBehaviorTree();
	}
}

void UAbilityCharacterState_Spawn::OnTermination()
{
	Super::OnTermination();
}

void UAbilityCharacterState_Spawn::TryLeave()
{
	Switch(nullptr);
}

// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Character/States/AbilityCharacterState_Spawn.h"
#include "Ability/Character/States/AbilityCharacterState_Death.h"
#include "Ability/Components/AbilitySystemComponentBase.h"
#include "Character/Components/CharacterMovementComponentBase.h"

#include "Ability/Character/AbilityCharacterBase.h"
#include "AI/Base/AIControllerBase.h"
#include "Common/Interaction/InteractionComponent.h"
#include "Common/Looking/LookingComponent.h"
#include "Components/CapsuleComponent.h"
#include "Event/EventModuleStatics.h"
#include "Event/Events/Ability/Event_VitalitySpawned.h"
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
	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();
	Rescuer = InParams.IsValidIndex(0) ? InParams[0].Get<UObject*>() : nullptr;
	bReviving = InLastState && InLastState->IsA<UAbilityCharacterState_Death>();
	Character->OnLifecycleSuspended(false);
	MovementSource = FGuid::NewGuid();
	FCharacterMovementConstraint Constraint;
	Constraint.bBlockMovement = true;
	Constraint.bBlockRotation = true;
	CastChecked<UCharacterMovementComponentBase>(Character->GetCharacterMovement())->SetMovementConstraint(MovementSource, this, Constraint);
	Character->GetCharacterMovement()->StopMovementImmediately();
	Character->GetCharacterMovement()->DisableMovement();
	Character->GetCharacterMovement()->Deactivate();
	Character->GetCollisionComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void UAbilityCharacterState_Spawn::OnRefresh(float DeltaSeconds)
{
	Super::OnRefresh(DeltaSeconds);

	TryLeave();
}

void UAbilityCharacterState_Spawn::OnLeave(UFiniteStateBase* InNextState)
{
	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();
	CastChecked<UCharacterMovementComponentBase>(Character->GetCharacterMovement())->ReleaseMovementSource(MovementSource, this);
	MovementSource.Invalidate();
	if (InNextState && InNextState->GetStateName() == TEXT("Alive"))
	{
		if (bReviving)
		{
			Character->ResetData();
		}
		UEventModuleStatics::BroadcastEvent<FEventVitalitySpawned>(this, {Character, Rescuer.Get()});
	}
	Rescuer.Reset();
	Super::OnLeave(InNextState);
}

void UAbilityCharacterState_Spawn::OnTermination()
{
	if (AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>())
	{
		CastChecked<UCharacterMovementComponentBase>(Character->GetCharacterMovement())->ReleaseMovementSource(MovementSource, this);
	}
	MovementSource.Invalidate();
	Rescuer.Reset();
	Super::OnTermination();
}

void UAbilityCharacterState_Spawn::TryLeave()
{
	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();
	if (Character && Character->IsAbilityRuntimeReady())
	{
		Character->GetFSMComponent()->SwitchStateByName(TEXT("Alive"));
	}
}

void UAbilityCharacterState_Spawn::OnPresentationEnter(UFiniteStateBase* Last, const FFSMReplicatedState& State)
{
    Super::OnPresentationEnter(Last, State);
    if (AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>())
    {
        Character->GetCharacterMovement()->StopMovementImmediately();
        Character->GetCharacterMovement()->DisableMovement();
        Character->GetCharacterMovement()->Deactivate();
        Character->GetCollisionComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Character->Execute_SetActorVisible(Character, false);
        if (Character->IsLocallyControlled()) Character->Execute_SetBlockAllInput(Character, true);
    }
}

void UAbilityCharacterState_Spawn::OnPresentationLeave(UFiniteStateBase* Next)
{
    if (AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>())
    {
        Character->Execute_SetActorVisible(Character, true);
    }
    Super::OnPresentationLeave(Next);
}

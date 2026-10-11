// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Character/States/AbilityCharacterState_Death.h"

#include "AbilitySystemComponent.h"
#include "Ability/Components/AbilitySystemComponentBase.h"
#include "Character/Components/CharacterMovementComponentBase.h"
#include "FSM/Components/FSMComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Ability/Character/AbilityCharacterBase.h"
#include "Ability/Character/States/AbilityCharacterState_Spawn.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ObjectPool/ObjectPoolModuleStatics.h"
#include "AI/Base/AIControllerBase.h"
#include "Common/Interaction/InteractionComponent.h"
#include "Common/Looking/LookingComponent.h"
#include "Event/EventModuleStatics.h"
#include "Event/Events/Ability/Event_VitalityDead.h"

UAbilityCharacterState_Death::UAbilityCharacterState_Death()
{
	StateName = FName("Death");

	Killer = nullptr;
	bDeathStarted = false;
}

void UAbilityCharacterState_Death::OnInitialize(UFSMComponent* InFSM, int32 InStateIndex)
{
	Super::OnInitialize(InFSM, InStateIndex);
}

bool UAbilityCharacterState_Death::OnPreEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	return Super::OnPreEnter(InLastState, InParams);
}

void UAbilityCharacterState_Death::OnEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	Super::OnEnter(InLastState, InParams);
	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();
	Character->GetCharacterMovement()->Activate(true);
	Character->GetCharacterMovement()->SetComponentTickEnabled(true);
	Killer = InParams.IsValidIndex(0) ? InParams[0].Get<UObject*>() : nullptr;
	bDeathStarted = false;
	bRestoringDeath = InParams.IsValidIndex(1) && InParams[1].Get<bool>();
	bDeathEnded = bRestoringDeath;
	Character->OnLifecycleSuspended(true);
	Character->GetAbilitySystemComponent()->SetLooseGameplayTagCount(GameplayTags::State_Vitality_Dead, 1, EGameplayTagReplicationState::TagAndCountToAll);
	Character->ApplyDeathAttributes();
	if (AAIControllerBase* Controller = Cast<AAIControllerBase>(Character->GetController()))
	{
		Controller->StopBehaviorTree();
	}
	MovementSource = FGuid::NewGuid();
	FCharacterMovementConstraint Constraint;
	Constraint.bBlockMovement = true;
	Constraint.bBlockRotation = true;
	CastChecked<UCharacterMovementComponentBase>(Character->GetCharacterMovement())->SetMovementConstraint(MovementSource, this, Constraint);
	if (!bRestoringDeath) UEventModuleStatics::BroadcastEvent<FEventVitalityDead>(this, {Character, Killer.Get()});
	DeathStart();
}

void UAbilityCharacterState_Death::OnRefresh(float DeltaSeconds)
{
	Super::OnRefresh(DeltaSeconds);
	if (!bDeathEnded && IsDeathPresentationFinished())
	{
		DeathEnd();
	}
}

bool UAbilityCharacterState_Death::OnPreLeave(UFiniteStateBase* InNextState)
{
	if (!Super::OnPreLeave(InNextState))
		return false;

	return InNextState && InNextState->IsA<UAbilityCharacterState_Spawn>();
}

void UAbilityCharacterState_Death::OnLeave(UFiniteStateBase* InNextState)
{
	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();
	CastChecked<UCharacterMovementComponentBase>(Character->GetCharacterMovement())->ReleaseMovementSource(MovementSource, this);
	MovementSource.Invalidate();
	Killer.Reset();
	bDeathStarted = false;
	bDeathEnded = false;
	bRestoringDeath = false;
	Super::OnLeave(InNextState);
}

void UAbilityCharacterState_Death::OnTermination()
{
	if (AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>())
	{
		CastChecked<UCharacterMovementComponentBase>(Character->GetCharacterMovement())->ReleaseMovementSource(MovementSource, this);
	}
	MovementSource.Invalidate();
	Killer.Reset();
	Super::OnTermination();
}

void UAbilityCharacterState_Death::DeathStart()
{
	bDeathStarted = true;
}

void UAbilityCharacterState_Death::DeathEnd()
{
	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();
	if (!Character || !Character->HasAuthority() || bDeathEnded)
	{
		return;
	}
	bDeathEnded = true;

	if (!Character->IsPlayer())
	{
		UObjectPoolModuleStatics::DespawnObject(Character);
	}
}


bool UAbilityCharacterState_Death::IsDeathPresentationFinished() const
{
	if (bRestoringDeath) return true;
	const AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();
	if (!Character || !Character->GetWorld())
	{
		return false;
	}
	const AGameStateBase* GameState = Character->GetWorld()->GetGameState();
	const double Time = GameState ? GameState->GetServerWorldTimeSeconds() : Character->GetWorld()->GetTimeSeconds();
	return Time - Character->GetFSMComponent()->GetReplicatedState().StartedAt >= DeathPresentationDuration;
}

void UAbilityCharacterState_Death::OnPresentationEnter(UFiniteStateBase* Last, const FFSMReplicatedState& State)
{
	Super::OnPresentationEnter(Last, State);
	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();
	Character->GetCharacterMovement()->Activate(true);
	Character->GetCharacterMovement()->SetComponentTickEnabled(true);
	if (DeathMontage && Character->GetMesh()->GetAnimInstance() && !IsDeathPresentationFinished())
	{
		const AGameStateBase* GameState = Character->GetWorld()->GetGameState();
		const double Time = GameState ? GameState->GetServerWorldTimeSeconds() : Character->GetWorld()->GetTimeSeconds();
		Character->GetMesh()->GetAnimInstance()->Montage_Play(DeathMontage, 1.f, EMontagePlayReturnType::MontageLength, FMath::Max(0.0, Time - State.StartedAt));
	}
}

void UAbilityCharacterState_Death::OnPresentationLeave(UFiniteStateBase* Next)
{
	AAbilityCharacterBase* Character = GetAgent<AAbilityCharacterBase>();
	if (DeathMontage && Character && Character->GetMesh()->GetAnimInstance())
	{
		Character->GetMesh()->GetAnimInstance()->Montage_Stop(0.1f, DeathMontage);
	}
	Super::OnPresentationLeave(Next);
}

// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Pawn/States/AbilityPawnState_Death.h"

#include "AbilitySystemComponent.h"
#include "Ability/Pawn/AbilityPawnBase.h"
#include "Ability/Pawn/States/AbilityPawnState_Spawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "ObjectPool/ObjectPoolModuleStatics.h"
#include "AI/Base/AIControllerBase.h"
#include "Common/Interaction/InteractionComponent.h"
#include "Event/EventModuleStatics.h"
#include "Event/Events/Ability/Event_VitalityDead.h"

UAbilityPawnState_Death::UAbilityPawnState_Death()
{
	StateName = FName("Death");

	KillerActor.Reset();
}

void UAbilityPawnState_Death::OnInitialize(UFSMComponent* InFSM, int32 InStateIndex)
{
	Super::OnInitialize(InFSM, InStateIndex);
}

bool UAbilityPawnState_Death::OnPreEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	return Super::OnPreEnter(InLastState, InParams);
}

void UAbilityPawnState_Death::OnEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	Super::OnEnter(InLastState, InParams);

	if (InParams.IsValidIndex(0))
	{
		KillerActor = Cast<AActor>(InParams[0].Get<UObject*>());
	}

	AAbilityPawnBase* Pawn = GetAgent<AAbilityPawnBase>();

	SetStateTag(GameplayTags::State_Vitality_Active, false);
	SetStateTag(GameplayTags::State_Vitality_Dying, true);

	Pawn->GetInteractionComponent()->SetInteractable(false);

	if (Pawn->GetController<AAIControllerBase>())
	{
		Pawn->GetController<AAIControllerBase>()->StopBehaviorTree();
	}

	Pawn->SetExp(0.f);
	Pawn->SetHealth(0.f);

	DeathStart();
	UEventModuleStatics::BroadcastEvent<FEventVitalityDead>(this, {GetAgent(), KillerActor.Get()});
}

void UAbilityPawnState_Death::OnRefresh(float DeltaSeconds)
{
	Super::OnRefresh(DeltaSeconds);
	if (!bDeathFinalized && FMath::IsFinite(DeltaSeconds) && DeltaSeconds > 0.f)
	{
		RemainingLifetime = FMath::Max(0.f, RemainingLifetime - DeltaSeconds);
		if (RemainingLifetime <= 0.f)
		{
			DeathEnd();
		}
	}
}

bool UAbilityPawnState_Death::OnPreLeave(UFiniteStateBase* InNextState)
{
	if (!Super::OnPreLeave(InNextState))
		return false;

	return InNextState && InNextState->IsA<UAbilityPawnState_Spawn>();
}

void UAbilityPawnState_Death::OnLeave(UFiniteStateBase* InNextState)
{
	Super::OnLeave(InNextState);

	KillerActor.Reset();

	AAbilityPawnBase* Pawn = GetAgent<AAbilityPawnBase>();

	SetStateTag(GameplayTags::State_Vitality_Dying, false);
	SetStateTag(GameplayTags::State_Vitality_Dead, false);


	if (Pawn->IsPlayer())
	{
		Pawn->Execute_SetActorVisible(Pawn, true);
	}
}

void UAbilityPawnState_Death::OnTermination()
{
	Super::OnTermination();
}

void UAbilityPawnState_Death::DeathStart()
{
	AAbilityPawnBase* Pawn = GetAgent<AAbilityPawnBase>();
	if (!Pawn || !Pawn->HasAuthority())
	{
		return;
	}
	bDeathFinalized = false;
	RemainingLifetime = FMath::IsFinite(CorpseLifetime) ? FMath::Max(0.f, CorpseLifetime) : 1.5f;
	Pawn->GetCollisionComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (Pawn->GetMovementComponent())
	{
		Pawn->GetMovementComponent()->StopMovementImmediately();
		if (Pawn->GetMovementComponent()) { Pawn->GetMovementComponent()->SetActive(false); }
	}
}

void UAbilityPawnState_Death::DeathEnd()
{
	if (bDeathFinalized || !GetAgent() || !GetAgent()->HasAuthority())
	{
		return;
	}
	bDeathFinalized = true;

	AAbilityPawnBase* Pawn = GetAgent<AAbilityPawnBase>();

	SetStateTag(GameplayTags::State_Vitality_Dying, false);
	SetStateTag(GameplayTags::State_Vitality_Dead, true);


	if (!Pawn->IsPlayer())
	{
		UObjectPoolModuleStatics::DespawnObject(Pawn);
	}
	else
	{
		Pawn->Execute_SetActorVisible(Pawn, false);
	}
}

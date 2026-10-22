// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Vitality/States/AbilityVitalityState_Death.h"

#include "AbilitySystemComponent.h"
#include "Ability/Vitality/AbilityVitalityBase.h"
#include "Ability/Vitality/States/AbilityVitalityState_Spawn.h"
#include "ObjectPool/ObjectPoolModuleStatics.h"
#include "Common/Interaction/InteractionComponent.h"
#include "Event/EventModuleStatics.h"
#include "Event/Events/Ability/Event_VitalityDead.h"

UAbilityVitalityState_Death::UAbilityVitalityState_Death()
{
	StateName = FName("Death");

	KillerActor.Reset();
}

void UAbilityVitalityState_Death::OnInitialize(UFSMComponent* InFSM, int32 InStateIndex)
{
	Super::OnInitialize(InFSM, InStateIndex);
}

bool UAbilityVitalityState_Death::OnPreEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	return Super::OnPreEnter(InLastState, InParams);
}

void UAbilityVitalityState_Death::OnEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	Super::OnEnter(InLastState, InParams);

	if (InParams.IsValidIndex(0))
	{
		KillerActor = Cast<AActor>(InParams[0].Get<UObject*>());
	}

	AAbilityVitalityBase* Vitality = GetAgent<AAbilityVitalityBase>();

	SetStateTag(GameplayTags::State_Vitality_Active, false);
	SetStateTag(GameplayTags::State_Vitality_Dying, true);

	Vitality->GetInteractionComponent()->SetInteractable(false);

	Vitality->SetExp(0.f);
	Vitality->SetHealth(0.f);

	DeathStart();
	UEventModuleStatics::BroadcastEvent<FEventVitalityDead>(this, {GetAgent(), KillerActor.Get()});
}

void UAbilityVitalityState_Death::OnRefresh(float DeltaSeconds)
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

bool UAbilityVitalityState_Death::OnPreLeave(UFiniteStateBase* InNextState)
{
	if (!Super::OnPreLeave(InNextState))
		return false;

	return InNextState && InNextState->IsA<UAbilityVitalityState_Spawn>();
}

void UAbilityVitalityState_Death::OnLeave(UFiniteStateBase* InNextState)
{
	Super::OnLeave(InNextState);

	KillerActor.Reset();

	AAbilityVitalityBase* Vitality = GetAgent<AAbilityVitalityBase>();

	SetStateTag(GameplayTags::State_Vitality_Dying, false);
	SetStateTag(GameplayTags::State_Vitality_Dead, false);

}

void UAbilityVitalityState_Death::OnTermination()
{
	Super::OnTermination();
}

void UAbilityVitalityState_Death::DeathStart()
{
	AAbilityVitalityBase* Vitality = GetAgent<AAbilityVitalityBase>();
	if (!Vitality || !Vitality->HasAuthority())
	{
		return;
	}
	bDeathFinalized = false;
	RemainingLifetime = FMath::IsFinite(CorpseLifetime) ? FMath::Max(0.f, CorpseLifetime) : 1.5f;
	Vitality->GetCollisionComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void UAbilityVitalityState_Death::DeathEnd()
{
	if (bDeathFinalized || !GetAgent() || !GetAgent()->HasAuthority())
	{
		return;
	}
	bDeathFinalized = true;

	AAbilityVitalityBase* Vitality = GetAgent<AAbilityVitalityBase>();

	SetStateTag(GameplayTags::State_Vitality_Dying, false);
	SetStateTag(GameplayTags::State_Vitality_Dead, true);


	UObjectPoolModuleStatics::DespawnObject(Vitality);
}

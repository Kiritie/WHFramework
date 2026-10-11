// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Vitality/States/AbilityVitalityStateBase.h"
#include "Ability/Actor/AbilityActorInterface.h"
#include "AbilitySystemComponent.h"
#include "Components/ShapeComponent.h"

UAbilityVitalityStateBase::UAbilityVitalityStateBase()
{
	StateName = FName("AbilityVitalityStateBase");
	StateDomain = TEXT("Lifecycle");
}

void UAbilityVitalityStateBase::OnInitialize(UFSMComponent* InFSM, int32 InStateIndex)
{
	Super::OnInitialize(InFSM, InStateIndex);
}

bool UAbilityVitalityStateBase::OnPreEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	return Super::OnPreEnter(InLastState, InParams);
}

void UAbilityVitalityStateBase::OnEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	Super::OnEnter(InLastState, InParams);
}

void UAbilityVitalityStateBase::OnRefresh(float DeltaSeconds)
{
	Super::OnRefresh(DeltaSeconds);
}

void UAbilityVitalityStateBase::OnLeave(UFiniteStateBase* InNextState)
{
	ClearStateTags();
	Super::OnLeave(InNextState);
}

void UAbilityVitalityStateBase::OnTermination()
{
	ClearStateTags();
	Super::OnTermination();
}


void UAbilityVitalityStateBase::SetStateTag(FGameplayTag Tag, bool bPresent)
{
	AActor* Actor = GetAgent();
	IAbilityActorInterface* Agent = Cast<IAbilityActorInterface>(Actor);
	UAbilitySystemComponent* AbilitySystem = Agent ? Agent->GetAbilitySystemComponent() : nullptr;
	if (!Actor || !Actor->HasAuthority() || !AbilitySystem || !Tag.IsValid())
	{
		return;
	}
	AbilitySystem->SetLooseGameplayTagCount(Tag, bPresent ? 1 : 0, EGameplayTagReplicationState::TagAndCountToAll);
	if (bPresent)
	{
		StateTags.AddTag(Tag);
	}
	else
	{
		StateTags.RemoveTag(Tag);
	}
}

void UAbilityVitalityStateBase::ClearStateTags()
{
	const FGameplayTagContainer Previous = StateTags;
	for (FGameplayTag Tag : Previous)
	{
		SetStateTag(Tag, false);
	}
	StateTags.Reset();
}

void UAbilityVitalityStateBase::OnPresentationEnter(UFiniteStateBase* InLastState, const FFSMReplicatedState& State)
{
	Super::OnPresentationEnter(InLastState, State);
	IAbilityActorInterface* Agent = Cast<IAbilityActorInterface>(GetAgent());
	if (State.Domain == TEXT("Lifecycle") && Agent && Agent->GetCollisionComponent())
	{
		Agent->GetCollisionComponent()->SetCollisionEnabled(State.State == TEXT("Active") ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	}
}

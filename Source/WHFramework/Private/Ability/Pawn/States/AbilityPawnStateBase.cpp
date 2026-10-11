// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Pawn/States/AbilityPawnStateBase.h"
#include "Ability/Actor/AbilityActorInterface.h"
#include "AbilitySystemComponent.h"
#include "Components/ShapeComponent.h"

UAbilityPawnStateBase::UAbilityPawnStateBase()
{
	StateName = FName("AbilityPawnStateBase");
	StateDomain = TEXT("Lifecycle");
}

void UAbilityPawnStateBase::OnInitialize(UFSMComponent* InFSM, int32 InStateIndex)
{
	Super::OnInitialize(InFSM, InStateIndex);
}

bool UAbilityPawnStateBase::OnPreEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	return Super::OnPreEnter(InLastState, InParams);
}

void UAbilityPawnStateBase::OnEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	Super::OnEnter(InLastState, InParams);
}

void UAbilityPawnStateBase::OnRefresh(float DeltaSeconds)
{
	Super::OnRefresh(DeltaSeconds);
}

void UAbilityPawnStateBase::OnLeave(UFiniteStateBase* InNextState)
{
	ClearStateTags();
	Super::OnLeave(InNextState);
}

void UAbilityPawnStateBase::OnTermination()
{
	ClearStateTags();
	Super::OnTermination();
}


void UAbilityPawnStateBase::SetStateTag(FGameplayTag Tag, bool bPresent)
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

void UAbilityPawnStateBase::ClearStateTags()
{
	const FGameplayTagContainer Previous = StateTags;
	for (FGameplayTag Tag : Previous)
	{
		SetStateTag(Tag, false);
	}
	StateTags.Reset();
}

void UAbilityPawnStateBase::OnPresentationEnter(UFiniteStateBase* InLastState, const FFSMReplicatedState& State)
{
	Super::OnPresentationEnter(InLastState, State);
	IAbilityActorInterface* Agent = Cast<IAbilityActorInterface>(GetAgent());
	if (State.Domain == TEXT("Lifecycle") && Agent && Agent->GetCollisionComponent())
	{
		Agent->GetCollisionComponent()->SetCollisionEnabled(State.State == TEXT("Active") ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	}
}

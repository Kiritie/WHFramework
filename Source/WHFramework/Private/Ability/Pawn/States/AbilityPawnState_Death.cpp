// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Pawn/States/AbilityPawnState_Death.h"

#include "AbilitySystemComponent.h"
#include "Ability/Pawn/AbilityPawnBase.h"
#include "AI/Base/AIControllerBase.h"
#include "ObjectPool/ObjectPoolModuleStatics.h"

UAbilityPawnState_Death::UAbilityPawnState_Death()
{
	StateName = FName("Death");
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

	if(InParams.IsValidIndex(0))
	{
		Killer = InParams[0].GetPointerValue<IAbilityVitalityInterface>();
	}

	AAbilityPawnBase* Pawn = GetAgent<AAbilityPawnBase>();

	Pawn->GetAbilitySystemComponent()->AddLooseGameplayTag(GameplayTags::State_Vitality_Dying);

	if(!Pawn->IsPlayer())
	{
		Pawn->GetController<AAIControllerBase>()->StopBehaviorTree();
	}

	if(Killer && Killer != Pawn)
	{
		Killer->ModifyExp(Pawn->GetLevelA() * 5.f);
	}

	Pawn->SetExp(0);
	Pawn->SetHealth(0.f);

	DeathStart();
}

void UAbilityPawnState_Death::OnRefresh(float DeltaSeconds)
{
	Super::OnRefresh(DeltaSeconds);
}

void UAbilityPawnState_Death::OnLeave(UFiniteStateBase* InNextState)
{
	Super::OnLeave(InNextState);

	Killer = nullptr;

	AAbilityPawnBase* Pawn = GetAgent<AAbilityPawnBase>();

	Pawn->GetAbilitySystemComponent()->RemoveLooseGameplayTag(GameplayTags::State_Vitality_Dying);
	Pawn->GetAbilitySystemComponent()->RemoveLooseGameplayTag(GameplayTags::State_Vitality_Dead);
}

void UAbilityPawnState_Death::OnTermination()
{
	Super::OnTermination();
}

void UAbilityPawnState_Death::DeathStart()
{
	DeathEnd();
}

void UAbilityPawnState_Death::DeathEnd()
{
	AAbilityPawnBase* Pawn = GetAgent<AAbilityPawnBase>();
	
	Pawn->GetAbilitySystemComponent()->RemoveLooseGameplayTag(GameplayTags::State_Vitality_Dying);
	
	Pawn->GetAbilitySystemComponent()->AddLooseGameplayTag(GameplayTags::State_Vitality_Dead);

	UObjectPoolModuleStatics::DespawnObject(Pawn);
}

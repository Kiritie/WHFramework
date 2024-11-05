// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Pawn/States/AbilityPawnState_Death.h"

#include "AbilitySystemComponent.h"
#include "Ability/Pawn/AbilityPawnBase.h"
#include "Ability/Pawn/AbilityPawnInventoryBase.h"
#include "GameFramework/PawnMovementComponent.h"
#include "ObjectPool/ObjectPoolModuleStatics.h"
#include "AI/Base/AIControllerBase.h"
#include "Common/Interaction/InteractionComponent.h"

UAbilityPawnState_Death::UAbilityPawnState_Death()
{
	StateName = FName("Death");

	Killer = nullptr;
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

	if(Pawn->GetController<AAIControllerBase>())
	{
		Pawn->GetController<AAIControllerBase>()->StopBehaviorTree();
	}

	if(Killer && Killer != Pawn)
	{
		Killer->ModifyExp(Pawn->GetLevelA() * 10.f);
	}

	Pawn->SetExp(0.f);
	Pawn->SetHealth(0.f);

	DeathStart();
}

void UAbilityPawnState_Death::OnRefresh(float DeltaSeconds)
{
	Super::OnRefresh(DeltaSeconds);
}

bool UAbilityPawnState_Death::OnPreLeave(UFiniteStateBase* InNextState)
{
	if(!Super::OnPreLeave(InNextState)) return false;

	return InNextState && InNextState->IsA<UAbilityPawnState_Spawn>();
}

void UAbilityPawnState_Death::OnLeave(UFiniteStateBase* InNextState)
{
	Super::OnLeave(InNextState);

	Killer = nullptr;

	AAbilityPawnBase* Pawn = GetAgent<AAbilityPawnBase>();

	Pawn->GetAbilitySystemComponent()->RemoveLooseGameplayTag(GameplayTags::State_Vitality_Dying);
	Pawn->GetAbilitySystemComponent()->RemoveLooseGameplayTag(GameplayTags::State_Vitality_Dead);

	Pawn->StopAction(GameplayTags::Ability_Vitality_Action_Death);
}

void UAbilityPawnState_Death::OnTermination()
{
	Super::OnTermination();
}

void UAbilityPawnState_Death::DeathStart()
{
	AAbilityPawnBase* Pawn = GetAgent<AAbilityPawnBase>();

	Pawn->GetMovementComponent()->SetActive(false);
	Pawn->GetCollisionComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Pawn->GetInteractionComponent()->SetInteractable(false);

	if(!Pawn->DoAction(GameplayTags::Ability_Vitality_Action_Death))
	{
		DeathEnd();
	}
}

void UAbilityPawnState_Death::DeathEnd()
{
	AAbilityPawnBase* Pawn = GetAgent<AAbilityPawnBase>();
	
	Pawn->GetAbilitySystemComponent()->RemoveLooseGameplayTag(GameplayTags::State_Vitality_Dying);
	Pawn->GetAbilitySystemComponent()->AddLooseGameplayTag(GameplayTags::State_Vitality_Dead);

	Pawn->Inventory->DiscardItems();

	if(!Pawn->IsPlayer())
	{
		UObjectPoolModuleStatics::DespawnObject(Pawn);
	}
	else
	{
		Pawn->Execute_SetActorVisible(Pawn, false);
	}
}

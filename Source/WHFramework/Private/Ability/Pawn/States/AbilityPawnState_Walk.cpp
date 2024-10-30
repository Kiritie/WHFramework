// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Pawn/States/AbilityPawnState_Walk.h"
#include "Ability/Pawn/AbilityPawnBase.h"
#include "ObjectPool/ObjectPoolModuleStatics.h"

UAbilityPawnState_Walk::UAbilityPawnState_Walk()
{
	StateName = FName("Walk");
}

void UAbilityPawnState_Walk::OnInitialize(UFSMComponent* InFSM, int32 InStateIndex)
{
	Super::OnInitialize(InFSM, InStateIndex);
}

bool UAbilityPawnState_Walk::OnPreEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	if(!Super::OnPreEnter(InLastState, InParams)) return false;

	AAbilityPawnBase* Pawn = GetAgent<AAbilityPawnBase>();

	return Pawn->DoAction(GameplayTags::Ability_Pawn_Action_Walk);
}

void UAbilityPawnState_Walk::OnEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	Super::OnEnter(InLastState, InParams);

	AAbilityPawnBase* Pawn = GetAgent<AAbilityPawnBase>();

	Pawn->GetAbilitySystemComponent()->AddLooseGameplayTag(GameplayTags::State_Vitality_Walking);
}

void UAbilityPawnState_Walk::OnRefresh(float DeltaSeconds)
{
	Super::OnRefresh(DeltaSeconds);
}

void UAbilityPawnState_Walk::OnLeave(UFiniteStateBase* InNextState)
{
	Super::OnLeave(InNextState);

	AAbilityPawnBase* Pawn = GetAgent<AAbilityPawnBase>();

	Pawn->GetAbilitySystemComponent()->RemoveLooseGameplayTag(GameplayTags::State_Vitality_Walking);

	Pawn->StopAction(GameplayTags::Ability_Pawn_Action_Walk);
}

void UAbilityPawnState_Walk::OnTermination()
{
	Super::OnTermination();
}

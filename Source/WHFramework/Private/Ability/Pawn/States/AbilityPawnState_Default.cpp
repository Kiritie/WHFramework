// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Pawn/States/AbilityPawnState_Default.h"

#include "Ability/Pawn/AbilityPawnBase.h"

UAbilityPawnState_Default::UAbilityPawnState_Default()
{
	StateName = FName("Default");
}

void UAbilityPawnState_Default::OnInitialize(UFSMComponent* InFSM, int32 InStateIndex)
{
	Super::OnInitialize(InFSM, InStateIndex);
}

bool UAbilityPawnState_Default::OnEnterValidate(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	return Super::OnEnterValidate(InLastState, InParams);
}

void UAbilityPawnState_Default::OnEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	Super::OnEnter(InLastState, InParams);

	AAbilityPawnBase* Pawn = GetAgent<AAbilityPawnBase>();

	Pawn->ResetData();
}

void UAbilityPawnState_Default::OnRefresh()
{
	Super::OnRefresh();
}

void UAbilityPawnState_Default::OnLeave(UFiniteStateBase* InNextState)
{
	Super::OnLeave(InNextState);
}

void UAbilityPawnState_Default::OnTermination()
{
	Super::OnTermination();
}

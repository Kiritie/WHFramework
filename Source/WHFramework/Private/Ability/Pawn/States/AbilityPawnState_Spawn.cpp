// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Pawn/States/AbilityPawnState_Spawn.h"

#include "Ability/Pawn/AbilityPawnBase.h"

UAbilityPawnState_Spawn::UAbilityPawnState_Spawn()
{
	StateName = FName("Default");
}

void UAbilityPawnState_Spawn::OnInitialize(UFSMComponent* InFSM, int32 InStateIndex)
{
	Super::OnInitialize(InFSM, InStateIndex);
}

bool UAbilityPawnState_Spawn::OnPreEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	return Super::OnPreEnter(InLastState, InParams);
}

void UAbilityPawnState_Spawn::OnEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	Super::OnEnter(InLastState, InParams);

	AAbilityPawnBase* Pawn = GetAgent<AAbilityPawnBase>();

	Pawn->ResetData();
}

void UAbilityPawnState_Spawn::OnRefresh(float DeltaSeconds)
{
	Super::OnRefresh(DeltaSeconds);
}

void UAbilityPawnState_Spawn::OnLeave(UFiniteStateBase* InNextState)
{
	Super::OnLeave(InNextState);
}

void UAbilityPawnState_Spawn::OnTermination()
{
	Super::OnTermination();
}

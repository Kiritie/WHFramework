// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Pawn/States/AbilityPawnStateBase.h"

UAbilityPawnStateBase::UAbilityPawnStateBase()
{
	StateName = FName("AbilityPawnStateBase");
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
	Super::OnLeave(InNextState);
}

void UAbilityPawnStateBase::OnTermination()
{
	Super::OnTermination();
}

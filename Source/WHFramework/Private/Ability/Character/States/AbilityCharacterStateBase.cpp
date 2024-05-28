// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Character/States/AbilityCharacterStateBase.h"

UAbilityCharacterStateBase::UAbilityCharacterStateBase()
{
	StateName = FName("AbilityCharacterStateBase");
}

void UAbilityCharacterStateBase::OnInitialize(UFSMComponent* InFSM, int32 InStateIndex)
{
	Super::OnInitialize(InFSM, InStateIndex);
}

bool UAbilityCharacterStateBase::OnEnterValidate(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	return Super::OnEnterValidate(InLastState, InParams);
}

void UAbilityCharacterStateBase::OnEnter(UFiniteStateBase* InLastState, const TArray<FParameter>& InParams)
{
	Super::OnEnter(InLastState, InParams);
}

void UAbilityCharacterStateBase::OnRefresh()
{
	Super::OnRefresh();
}

void UAbilityCharacterStateBase::OnLeave(UFiniteStateBase* InNextState)
{
	Super::OnLeave(InNextState);
}

void UAbilityCharacterStateBase::OnTermination()
{
	Super::OnTermination();
}

// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Character/States/AbilityCharacterStateBase.h"

UAbilityCharacterStateBase::UAbilityCharacterStateBase()
{
	StateName = FName("AbilityCharacterStateBase");
}

void UAbilityCharacterStateBase::OnInitialize(UFSMComponent* InFSMComponent, int32 InStateIndex)
{
	Super::OnInitialize(InFSMComponent, InStateIndex);
}

void UAbilityCharacterStateBase::OnEnter(UFiniteStateBase* InLastFiniteState)
{
	Super::OnEnter(InLastFiniteState);
}

void UAbilityCharacterStateBase::OnRefresh()
{
	Super::OnRefresh();
}

void UAbilityCharacterStateBase::OnLeave(UFiniteStateBase* InNextFiniteState)
{
	Super::OnLeave(InNextFiniteState);
}

void UAbilityCharacterStateBase::OnTermination()
{
	Super::OnTermination();
}

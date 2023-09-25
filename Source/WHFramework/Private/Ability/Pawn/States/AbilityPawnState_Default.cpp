// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Pawn/States/AbilityPawnState_Default.h"

#include "Ability/Pawn/AbilityPawnBase.h"

UAbilityPawnState_Default::UAbilityPawnState_Default()
{
	StateName = FName("Default");
}

void UAbilityPawnState_Default::OnInitialize(UFSMComponent* InFSMComponent, int32 InStateIndex)
{
	Super::OnInitialize(InFSMComponent, InStateIndex);
}

bool UAbilityPawnState_Default::OnEnterValidate(UFiniteStateBase* InLastFiniteState)
{
	return Super::OnEnterValidate(InLastFiniteState);
}

void UAbilityPawnState_Default::OnEnter(UFiniteStateBase* InLastFiniteState)
{
	Super::OnEnter(InLastFiniteState);

	AAbilityPawnBase* Pawn = GetAgent<AAbilityPawnBase>();

	Pawn->ResetData();
}

void UAbilityPawnState_Default::OnRefresh()
{
	Super::OnRefresh();
}

void UAbilityPawnState_Default::OnLeave(UFiniteStateBase* InNextFiniteState)
{
	Super::OnLeave(InNextFiniteState);
}

void UAbilityPawnState_Default::OnTermination()
{
	Super::OnTermination();
}

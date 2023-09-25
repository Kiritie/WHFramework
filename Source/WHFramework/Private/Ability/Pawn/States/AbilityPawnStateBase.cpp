// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Vitality/States/AbilityPawnStateBase.h"

UAbilityPawnStateBase::UAbilityPawnStateBase()
{
	StateName = FName("AbilityPawnStateBase");
}

void UAbilityPawnStateBase::OnInitialize(UFSMComponent* InFSMComponent, int32 InStateIndex)
{
	Super::OnInitialize(InFSMComponent, InStateIndex);
}

bool UAbilityPawnStateBase::OnEnterValidate(UFiniteStateBase* InLastFiniteState)
{
	return Super::OnEnterValidate(InLastFiniteState);
}

void UAbilityPawnStateBase::OnEnter(UFiniteStateBase* InLastFiniteState)
{
	Super::OnEnter(InLastFiniteState);
}

void UAbilityPawnStateBase::OnRefresh()
{
	Super::OnRefresh();
}

void UAbilityPawnStateBase::OnLeave(UFiniteStateBase* InNextFiniteState)
{
	Super::OnLeave(InNextFiniteState);
}

void UAbilityPawnStateBase::OnTermination()
{
	Super::OnTermination();
}

// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/FSM/EventHandle_FiniteStateEntered.h"

UEventHandle_FiniteStateEntered::UEventHandle_FiniteStateEntered()
{
	State = nullptr;
	FSM = nullptr;
}

void UEventHandle_FiniteStateEntered::OnDespawn_Implementation(bool bRecovery)
{
	State = nullptr;
	FSM = nullptr;
}

void UEventHandle_FiniteStateEntered::Parse_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		State = InParams[0].GetObjectValue<UFiniteStateBase>();
	}
	if(InParams.IsValidIndex(0))
	{
		FSM = InParams[0].GetObjectValue<UFSMComponent>();
	}
}

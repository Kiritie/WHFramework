// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/FSM/EventHandle_FiniteStateLeaved.h"

UEventHandle_FiniteStateLeaved::UEventHandle_FiniteStateLeaved()
{
	State = nullptr;
	FSM = nullptr;
}

void UEventHandle_FiniteStateLeaved::OnDespawn_Implementation(bool bRecovery)
{
	State = nullptr;
	FSM = nullptr;
}

void UEventHandle_FiniteStateLeaved::Parse_Implementation(const TArray<FParameter>& InParams)
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

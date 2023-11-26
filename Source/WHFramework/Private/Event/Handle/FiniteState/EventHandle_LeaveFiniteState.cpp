// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/FiniteState/EventHandle_LeaveFiniteState.h"

UEventHandle_LeaveFiniteState::UEventHandle_LeaveFiniteState()
{

}

void UEventHandle_LeaveFiniteState::OnDespawn_Implementation(bool bRecovery)
{
	State = nullptr;
	FSM = nullptr;
}

void UEventHandle_LeaveFiniteState::Fill_Implementation(const TArray<FParameter>& InParams)
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

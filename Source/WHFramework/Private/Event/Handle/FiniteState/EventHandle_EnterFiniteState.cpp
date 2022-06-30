// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/FiniteState/EventHandle_EnterFiniteState.h"

UEventHandle_EnterFiniteState::UEventHandle_EnterFiniteState()
{
	EventType = EEventType::Multicast;
	FiniteState = nullptr;
}

void UEventHandle_EnterFiniteState::OnDespawn_Implementation()
{
	FiniteState = nullptr;
	FSMComponent = nullptr;
}

void UEventHandle_EnterFiniteState::Fill_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		FiniteState = InParams[0].GetObjectValue<UFiniteStateBase>();
	}
	if(InParams.IsValidIndex(0))
	{
		FSMComponent = InParams[0].GetObjectValue<UFSMComponent>();
	}
}

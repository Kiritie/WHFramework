// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Step/EventHandle_StartStep.h"

UEventHandle_StartStep::UEventHandle_StartStep()
{
	EventType = EEventType::Multicast;
	RootStepIndex = -1;
}

void UEventHandle_StartStep::OnDespawn_Implementation()
{
	RootStepIndex = -1;
}

void UEventHandle_StartStep::Fill_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		RootStepIndex = InParams[0].GetIntegerValue();
	}
}

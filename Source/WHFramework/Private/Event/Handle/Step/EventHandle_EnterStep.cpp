// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Step/EventHandle_EnterStep.h"

UEventHandle_EnterStep::UEventHandle_EnterStep()
{
	EventType = EEventType::Multicast;
	Step = nullptr;
}

void UEventHandle_EnterStep::OnDespawn_Implementation(bool bRecovery)
{
	Step = nullptr;
}

void UEventHandle_EnterStep::Fill_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		Step = InParams[0].GetObjectValue<UStepBase>();
	}
}

// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Step/EventHandle_EnterStep.h"

UEventHandle_EnterStep::UEventHandle_EnterStep()
{
	EventType = EEventType::Multicast;
	Step = nullptr;
}

void UEventHandle_EnterStep::OnDespawn_Implementation()
{
	Step = nullptr;
}

void UEventHandle_EnterStep::Fill_Implementation(const TArray<FParameter>& InParameters)
{
	if(InParameters.IsValidIndex(0))
	{
		Step = InParameters[0].GetObjectValue<UStepBase>();
	}
}

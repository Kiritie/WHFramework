// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Step/EventHandle_CompleteStep.h"

UEventHandle_CompleteStep::UEventHandle_CompleteStep()
{
	EventType = EEventType::Multicast;
	Step = nullptr;
}

void UEventHandle_CompleteStep::OnDespawn_Implementation()
{
	Step = nullptr;
}

void UEventHandle_CompleteStep::Fill_Implementation(const TArray<FParameter>& InParameters)
{
	if(InParameters.IsValidIndex(0))
	{
		Step = InParameters[0].GetObjectValue<UStepBase>();
	}
}

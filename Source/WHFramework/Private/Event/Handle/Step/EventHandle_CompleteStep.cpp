// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Step/EventHandle_CompleteStep.h"

UEventHandle_CompleteStep::UEventHandle_CompleteStep()
{
	EventType = EEventType::Multicast;
	Step = nullptr;
}

void UEventHandle_CompleteStep::OnDespawn_Implementation(bool bRecovery)
{
	Step = nullptr;
}

void UEventHandle_CompleteStep::Fill_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		Step = InParams[0].GetObjectValue<UStepBase>();
	}
}

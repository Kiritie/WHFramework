// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Step/EventHandle_EndStep.h"

UEventHandle_EndStep::UEventHandle_EndStep()
{
	EventType = EEventType::Multicast;
}

void UEventHandle_EndStep::OnDespawn_Implementation()
{
}

void UEventHandle_EndStep::Fill_Implementation(const TArray<FParameter>& InParams)
{
}

// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Global/EventHandle_EndPlay.h"

UEventHandle_EndPlay::UEventHandle_EndPlay()
{
	EventType = EEventType::Multicast;

	bIsSimulating = false;
}

void UEventHandle_EndPlay::OnDespawn_Implementation()
{
	bIsSimulating = false;
}

void UEventHandle_EndPlay::Fill_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		bIsSimulating = InParams[0].GetBooleanValue();
	}
}

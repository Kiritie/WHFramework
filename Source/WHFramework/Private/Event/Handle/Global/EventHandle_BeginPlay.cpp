// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Global/EventHandle_BeginPlay.h"

UEventHandle_BeginPlay::UEventHandle_BeginPlay()
{
	EventType = EEventType::Multicast;

	bIsSimulating = false;
}

void UEventHandle_BeginPlay::OnDespawn_Implementation(bool bRecovery)
{
	bIsSimulating = false;
}

void UEventHandle_BeginPlay::Fill_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		bIsSimulating = InParams[0].GetBooleanValue();
	}
}

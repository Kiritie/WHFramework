// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Common/EventHandle_BeginPlay.h"

UEventHandle_BeginPlay::UEventHandle_BeginPlay()
{
	EventType = EEventType::Multicast;
}

void UEventHandle_BeginPlay::OnDespawn_Implementation(bool bRecovery)
{
}

void UEventHandle_BeginPlay::Fill_Implementation(const TArray<FParameter>& InParams)
{
}

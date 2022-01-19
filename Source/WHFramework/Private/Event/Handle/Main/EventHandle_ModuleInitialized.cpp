// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Main/EventHandle_ModuleInitialized.h"

UEventHandle_ModuleInitialized::UEventHandle_ModuleInitialized()
{
	EventType = EEventType::Multicast;
}

void UEventHandle_ModuleInitialized::OnDespawn_Implementation()
{
}

void UEventHandle_ModuleInitialized::Fill_Implementation(const TArray<FParameter>& InParameters)
{
}

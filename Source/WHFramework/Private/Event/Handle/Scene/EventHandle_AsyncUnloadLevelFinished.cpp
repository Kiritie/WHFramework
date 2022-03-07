// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Scene/EventHandle_AsyncUnloadLevelFinished.h"

UEventHandle_AsyncUnloadLevelFinished::UEventHandle_AsyncUnloadLevelFinished()
{
	EventType = EEventType::Multicast;

	LevelPath = NAME_None;
}

void UEventHandle_AsyncUnloadLevelFinished::OnDespawn_Implementation()
{
}

void UEventHandle_AsyncUnloadLevelFinished::Fill_Implementation(const TArray<FParameter>& InParameters)
{
	if(InParameters.IsValidIndex(0))
	{
		LevelPath = *InParameters[0].GetStringValue();
	}
}

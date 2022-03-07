// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Scene/EventHandle_AsyncLoadLevelFinished.h"

UEventHandle_AsyncLoadLevelFinished::UEventHandle_AsyncLoadLevelFinished()
{
	EventType = EEventType::Multicast;

	LevelPath = NAME_None;
}

void UEventHandle_AsyncLoadLevelFinished::OnDespawn_Implementation()
{
}

void UEventHandle_AsyncLoadLevelFinished::Fill_Implementation(const TArray<FParameter>& InParameters)
{
	if(InParameters.IsValidIndex(0))
	{
		LevelPath = *InParameters[0].GetStringValue();
	}
}

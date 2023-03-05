// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Scene/EventHandle_AsyncLoadLevelFinished.h"

UEventHandle_AsyncLoadLevelFinished::UEventHandle_AsyncLoadLevelFinished()
{
	EventType = EEventType::Multicast;

	LevelPath = NAME_None;
}

void UEventHandle_AsyncLoadLevelFinished::OnDespawn_Implementation(bool bRecovery)
{
}

void UEventHandle_AsyncLoadLevelFinished::Fill_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		LevelPath = *InParams[0].GetStringValue();
	}
}

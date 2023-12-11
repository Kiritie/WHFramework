// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Scene/EventHandle_AsyncUnloadLevelFinished.h"

UEventHandle_AsyncUnloadLevelFinished::UEventHandle_AsyncUnloadLevelFinished()
{
	LevelPath = NAME_None;
}

void UEventHandle_AsyncUnloadLevelFinished::OnDespawn_Implementation(bool bRecovery)
{
}

void UEventHandle_AsyncUnloadLevelFinished::Parse_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		LevelPath = *InParams[0].GetStringValue();
	}
}

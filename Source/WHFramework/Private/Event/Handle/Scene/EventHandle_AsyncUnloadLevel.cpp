// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Scene/EventHandle_AsyncUnloadLevel.h"

UEventHandle_AsyncUnloadLevel::UEventHandle_AsyncUnloadLevel()
{
	LevelObjectPtr = nullptr;
	LevelPath = NAME_None;
	FinishDelayTime = 1.f;
	bCreateLoadingWidget = false;
}

void UEventHandle_AsyncUnloadLevel::OnDespawn_Implementation(bool bRecovery)
{
}

void UEventHandle_AsyncUnloadLevel::Parse_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		LevelObjectPtr = InParams[0];
	}
	if(InParams.IsValidIndex(1))
	{
		LevelPath = InParams[1];
	}
	if(InParams.IsValidIndex(2))
	{
		FinishDelayTime = InParams[2];
	}
	if(InParams.IsValidIndex(3))
	{
		bCreateLoadingWidget = InParams[3];
	}
}

TArray<FParameter> UEventHandle_AsyncUnloadLevel::Pack_Implementation()
{
	return TArray<FParameter>{ LevelObjectPtr, LevelPath, FinishDelayTime, bCreateLoadingWidget };
}

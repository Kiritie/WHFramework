// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Scene/EventHandle_AsyncLoadLevel.h"

UEventHandle_AsyncLoadLevel::UEventHandle_AsyncLoadLevel()
{
	LevelPath = NAME_None;
	FinishDelayTime = 1.f;
	bCreateLoadingWidget = false;
}

void UEventHandle_AsyncLoadLevel::OnDespawn_Implementation(bool bRecovery)
{
}

void UEventHandle_AsyncLoadLevel::Parse_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		LevelPath = InParams[0];
	}
	if(InParams.IsValidIndex(1))
	{
		FinishDelayTime = InParams[1];
	}
	if(InParams.IsValidIndex(2))
	{
		bCreateLoadingWidget = InParams[2];
	}
}

TArray<FParameter> UEventHandle_AsyncLoadLevel::Pack_Implementation()
{
	return TArray<FParameter>{ LevelPath, FinishDelayTime, bCreateLoadingWidget };
}

// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Scene/EventHandle_AsyncLoadLevels.h"

UEventHandle_AsyncLoadLevels::UEventHandle_AsyncLoadLevels()
{
	SoftLevelPaths = TArray<FSoftLevelPath>();
	FinishDelayTime = 1.f;
	bCreateLoadingWidget = false;
}

void UEventHandle_AsyncLoadLevels::OnDespawn_Implementation(bool bRecovery)
{
}

void UEventHandle_AsyncLoadLevels::Parse_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		SoftLevelPaths = InParams[0].GetPointerValueRef<TArray<FSoftLevelPath>>();
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

TArray<FParameter> UEventHandle_AsyncLoadLevels::Pack_Implementation()
{
	return { &SoftLevelPaths, FinishDelayTime, bCreateLoadingWidget };
}

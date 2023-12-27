// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Common/Game/EventHandle_GamePaused.h"

UEventHandle_GamePaused::UEventHandle_GamePaused()
{
	PauseMode = EPauseMode::Default;
}

void UEventHandle_GamePaused::OnDespawn_Implementation(bool bRecovery)
{
	PauseMode = EPauseMode::Default;
}

void UEventHandle_GamePaused::Parse_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		PauseMode = (EPauseMode)InParams[0].GetIntegerValue();
	}
}

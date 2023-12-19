// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Common/EventHandle_GameUnPaused.h"

UEventHandle_GameUnPaused::UEventHandle_GameUnPaused()
{
	PauseMode = EPauseMode::Default;
}

void UEventHandle_GameUnPaused::OnDespawn_Implementation(bool bRecovery)
{
	PauseMode = EPauseMode::Default;
}

void UEventHandle_GameUnPaused::Parse_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		PauseMode = (EPauseMode)InParams[0].GetIntegerValue();
	}
}

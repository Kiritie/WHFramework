// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Common/EventHandle_PauseGame.h"

UEventHandle_PauseGame::UEventHandle_PauseGame()
{
	PauseMode = EPauseMode::Default;
}

void UEventHandle_PauseGame::OnDespawn_Implementation(bool bRecovery)
{
	PauseMode = EPauseMode::Default;
}

void UEventHandle_PauseGame::Parse_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		PauseMode = (EPauseMode)InParams[0].GetIntegerValue();
	}
}

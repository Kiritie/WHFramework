// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Global/EventHandle_PauseGame.h"

UEventHandle_PauseGame::UEventHandle_PauseGame()
{
	EventType = EEventType::Multicast;

	PauseGameMode = EPauseGameMode::Default;
}

void UEventHandle_PauseGame::OnDespawn_Implementation(bool bRecovery)
{
	PauseGameMode = EPauseGameMode::Default;
}

void UEventHandle_PauseGame::Fill_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		PauseGameMode = (EPauseGameMode)InParams[0].GetIntegerValue();
	}
}

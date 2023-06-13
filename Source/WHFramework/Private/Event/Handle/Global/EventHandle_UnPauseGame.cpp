// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Global/EventHandle_UnPauseGame.h"

UEventHandle_UnPauseGame::UEventHandle_UnPauseGame()
{
	EventType = EEventType::Multicast;

	PauseMode = EPauseMode::Default;
}

void UEventHandle_UnPauseGame::OnDespawn_Implementation(bool bRecovery)
{
	PauseMode = EPauseMode::Default;
}

void UEventHandle_UnPauseGame::Fill_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		PauseMode = (EPauseMode)InParams[0].GetIntegerValue();
	}
}

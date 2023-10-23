// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Common/EventHandle_StartGame.h"

UEventHandle_StartGame::UEventHandle_StartGame()
{
	EventType = EEventType::Multicast;
}

void UEventHandle_StartGame::OnDespawn_Implementation(bool bRecovery)
{
}

void UEventHandle_StartGame::Fill_Implementation(const TArray<FParameter>& InParams)
{
}

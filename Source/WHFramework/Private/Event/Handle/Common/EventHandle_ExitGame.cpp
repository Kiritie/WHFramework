// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Common/EventHandle_ExitGame.h"

UEventHandle_ExitGame::UEventHandle_ExitGame()
{
	bIsSimulating = false;
}

void UEventHandle_ExitGame::OnDespawn_Implementation(bool bRecovery)
{
	bIsSimulating = false;
}

void UEventHandle_ExitGame::Parse_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		bIsSimulating = InParams[0].GetBooleanValue();
	}
}

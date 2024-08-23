// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Common/Game/EventHandle_GamePreExit.h"

UEventHandle_GamePreExit::UEventHandle_GamePreExit()
{
	bIsSimulating = false;
}

void UEventHandle_GamePreExit::OnDespawn_Implementation(bool bRecovery)
{
	bIsSimulating = false;
}

void UEventHandle_GamePreExit::Parse_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		bIsSimulating = InParams[0];
	}
}

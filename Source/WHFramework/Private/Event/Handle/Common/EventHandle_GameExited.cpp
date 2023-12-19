// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Common/EventHandle_GameExited.h"

UEventHandle_GameExited::UEventHandle_GameExited()
{
	bIsSimulating = false;
}

void UEventHandle_GameExited::OnDespawn_Implementation(bool bRecovery)
{
	bIsSimulating = false;
}

void UEventHandle_GameExited::Parse_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		bIsSimulating = InParams[0].GetBooleanValue();
	}
}

// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Input/EventHandle_InputModeChanged.h"

UEventHandle_InputModeChanged::UEventHandle_InputModeChanged()
{
	InputMode = EInputMode::None;
}

void UEventHandle_InputModeChanged::OnDespawn_Implementation(bool bRecovery)
{
}

void UEventHandle_InputModeChanged::Parse_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		InputMode = *InParams[0].GetPointerValue<EInputMode>();
	}
}

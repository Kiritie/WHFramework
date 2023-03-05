// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Input/EventHandle_ChangeInputMode.h"

UEventHandle_ChangeInputMode::UEventHandle_ChangeInputMode()
{
	EventType = EEventType::Multicast;

	InputMode = EInputMode::None;
}

void UEventHandle_ChangeInputMode::OnDespawn_Implementation(bool bRecovery)
{
}

void UEventHandle_ChangeInputMode::Fill_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		InputMode = *InParams[0].GetPointerValue<EInputMode>();
	}
}

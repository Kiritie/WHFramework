// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Widget/EventHandle_UserWidgetStateChanged.h"

UEventHandle_UserWidgetStateChanged::UEventHandle_UserWidgetStateChanged()
{
	UserWidget = nullptr;
	WidgetState = EScreenWidgetState::None;
}

void UEventHandle_UserWidgetStateChanged::OnDespawn_Implementation(bool bRecovery)
{
	UserWidget = nullptr;
	WidgetState = EScreenWidgetState::None;
}

void UEventHandle_UserWidgetStateChanged::Parse_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		UserWidget = InParams[0];
	}
	if(InParams.IsValidIndex(1))
	{
		WidgetState = InParams[1].GetPointerValueRef<EScreenWidgetState>();
	}
}

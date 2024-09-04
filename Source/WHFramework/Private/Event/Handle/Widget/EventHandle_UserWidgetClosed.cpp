// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Widget/EventHandle_UserWidgetClosed.h"

UEventHandle_UserWidgetClosed::UEventHandle_UserWidgetClosed()
{
	UserWidget = nullptr;
}

void UEventHandle_UserWidgetClosed::OnDespawn_Implementation(bool bRecovery)
{
	UserWidget = nullptr;
}

void UEventHandle_UserWidgetClosed::Parse_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		UserWidget = InParams[0];
	}
}

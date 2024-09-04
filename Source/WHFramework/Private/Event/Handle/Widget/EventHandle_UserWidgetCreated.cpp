// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Widget/EventHandle_UserWidgetCreated.h"

UEventHandle_UserWidgetCreated::UEventHandle_UserWidgetCreated()
{
	UserWidget = nullptr;
}

void UEventHandle_UserWidgetCreated::OnDespawn_Implementation(bool bRecovery)
{
	UserWidget = nullptr;
}

void UEventHandle_UserWidgetCreated::Parse_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		UserWidget = InParams[0];
	}
}

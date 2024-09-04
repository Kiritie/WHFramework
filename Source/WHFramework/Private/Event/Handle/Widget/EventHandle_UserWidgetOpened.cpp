// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Widget/EventHandle_UserWidgetOpened.h"

UEventHandle_UserWidgetOpened::UEventHandle_UserWidgetOpened()
{
	UserWidget = nullptr;
}

void UEventHandle_UserWidgetOpened::OnDespawn_Implementation(bool bRecovery)
{
	UserWidget = nullptr;
}

void UEventHandle_UserWidgetOpened::Parse_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		UserWidget = InParams[0];
	}
}

// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Widget/EventHandle_SetWorldWidgetVisible.h"

#include "Widget/World/WorldWidgetBase.h"

UEventHandle_SetWorldWidgetVisible::UEventHandle_SetWorldWidgetVisible()
{
	WidgetClass = nullptr;
	bVisible = false;
}

void UEventHandle_SetWorldWidgetVisible::OnDespawn_Implementation(bool bRecovery)
{
	WidgetClass = nullptr;
	bVisible = false;
}

void UEventHandle_SetWorldWidgetVisible::Parse_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		WidgetClass = InParams[0].GetClassValue();
	}
	if(InParams.IsValidIndex(1))
	{
		bVisible = InParams[1].GetBooleanValue();
	}
}

TArray<FParameter> UEventHandle_SetWorldWidgetVisible::Pack_Implementation()
{
	return { WidgetClass.Get(), bVisible };
}

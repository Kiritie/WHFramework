// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Widget/EventHandle_CloseUserWidget.h"

#include "Widget/Screen/UMG/UserWidgetBase.h"

UEventHandle_CloseUserWidget::UEventHandle_CloseUserWidget()
{
	WidgetClass = nullptr;
	bInstant = false;
}

void UEventHandle_CloseUserWidget::OnDespawn_Implementation(bool bRecovery)
{
	WidgetClass = nullptr;
	bInstant = false;
}

void UEventHandle_CloseUserWidget::Parse_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		WidgetClass = InParams[0].GetClassValue();
	}
	if(InParams.IsValidIndex(1))
	{
		bInstant = InParams[1].GetBooleanValue();
	}
}

TArray<FParameter> UEventHandle_CloseUserWidget::Pack_Implementation()
{
	return { WidgetClass.Get(), bInstant };
}

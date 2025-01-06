// Fill out your copyright notice in the Description page of Project Settings.

#include "Common/Widget/WidgetUIMaskBase.h"

UWidgetUIMaskBase::UWidgetUIMaskBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	WidgetName = FName("UIMask");
	WidgetType = EWidgetType::Permanent;
	WidgetInputMode = EInputMode::UIOnly;

	SetIsFocusable(true);
}

void UWidgetUIMaskBase::OnCreate(UObject* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnCreate(InOwner, InParams);
}

void UWidgetUIMaskBase::OnInitialize(UObject* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnInitialize(InOwner, InParams);
}

void UWidgetUIMaskBase::OnOpen(const TArray<FParameter>& InParams, bool bInstant)
{
	Super::OnOpen(InParams, bInstant);

	if(InParams.IsValidIndex(0))
	{
		OnPressAnyKey = InParams[0];
	}
}

void UWidgetUIMaskBase::OnClose(bool bInstant)
{
	Super::OnClose(bInstant);

	OnPressAnyKey.Clear();
}

FReply UWidgetUIMaskBase::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if(OnPressAnyKey.IsBound())
	{
		OnPressAnyKey.Execute();
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UWidgetUIMaskBase::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if(OnPressAnyKey.IsBound())
	{
		OnPressAnyKey.Execute();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

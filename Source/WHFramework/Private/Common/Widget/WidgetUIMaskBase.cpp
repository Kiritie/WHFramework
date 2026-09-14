// Fill out your copyright notice in the Description page of Project Settings.

#include "Common/Widget/WidgetUIMaskBase.h"

UWidgetUIMaskBase::UWidgetUIMaskBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	SetIsFocusable(true);
	bWidgetActivatable = false;
}

void UWidgetUIMaskBase::OnCreate(const FParameter& InParam)
{
	Super::OnCreate(InParam);
}

void UWidgetUIMaskBase::OnInitialize(const FParameter& InParam)
{
	Super::OnInitialize(InParam);
}

void UWidgetUIMaskBase::OnOpen(const FParameter& InParam, bool bInstant)
{
	Super::OnOpen(InParam, bInstant);

	OnPressAnyKey.Clear();
	if(const FWidgetDelegateOpenParameter* Param = InParam.GetPtr<FWidgetDelegateOpenParameter>())
	{
		OnPressAnyKey = Param->Delegate;
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

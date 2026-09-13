// Fill out your copyright notice in the Description page of Project Settings.

#include "Scene/Widget/WidgetSceneMaskBase.h"

UWidgetSceneMaskBase::UWidgetSceneMaskBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

void UWidgetSceneMaskBase::OnCreate(const FParameter& InParam)
{
	Super::OnCreate(InParam);
}

void UWidgetSceneMaskBase::OnInitialize(const FParameter& InParam)
{
	Super::OnInitialize(InParam);
}

void UWidgetSceneMaskBase::OnOpen(const FParameter& InParam, bool bInstant)
{
	Super::OnOpen(InParam, bInstant);

	OnMaskClosed.Clear();
	if(const FWidgetDelegateOpenParameter* Param = InParam.GetPtr<FWidgetDelegateOpenParameter>())
	{
		OnMaskClosed = Param->Delegate;
	}
}

void UWidgetSceneMaskBase::OnClose(bool bInstant)
{
	Super::OnClose(bInstant);

	if(OnMaskClosed.IsBound())
	{
		OnMaskClosed.Execute();
	}
}

FReply UWidgetSceneMaskBase::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Close();
	return FReply::Handled();
}

FReply UWidgetSceneMaskBase::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	Close();
	return FReply::Handled();
}

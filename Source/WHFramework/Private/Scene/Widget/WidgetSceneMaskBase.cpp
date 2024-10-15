// Fill out your copyright notice in the Description page of Project Settings.

#include "Scene/Widget/WidgetSceneMaskBase.h"

UWidgetSceneMaskBase::UWidgetSceneMaskBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	WidgetName = FName("SceneMask");
	WidgetType = EWidgetType::Permanent;
	WidgetZOrder = -1;
	WidgetInputMode = EInputMode::UIOnly;
	SetIsFocusable(true);
}

void UWidgetSceneMaskBase::OnCreate(UObject* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnCreate(InOwner, InParams);
}

void UWidgetSceneMaskBase::OnInitialize(UObject* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnInitialize(InOwner, InParams);
}

void UWidgetSceneMaskBase::OnOpen(const TArray<FParameter>& InParams, bool bInstant)
{
	Super::OnOpen(InParams, bInstant);

	if(InParams.IsValidIndex(0))
	{
		OnMaskClosed = InParams[0];
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

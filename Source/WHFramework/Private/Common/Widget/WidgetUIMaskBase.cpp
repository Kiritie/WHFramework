// Fill out your copyright notice in the Description page of Project Settings.

#include "Common/Widget/WidgetUIMaskBase.h"

UWidgetUIMaskBase::UWidgetUIMaskBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	WidgetName = FName("UIMask");
	WidgetType = EWidgetType::Permanent;
	bWidgetPenetrable = false;
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
}

void UWidgetUIMaskBase::OnClose(bool bInstant)
{
	Super::OnClose(bInstant);
}

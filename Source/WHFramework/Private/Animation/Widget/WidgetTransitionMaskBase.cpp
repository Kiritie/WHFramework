// Fill out your copyright notice in the Description page of Project Settings.

#include "Animation/Widget/WidgetTransitionMaskBase.h"

UWidgetTransitionMaskBase::UWidgetTransitionMaskBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	WidgetName = FName("TransitionMask");

	WidgetType = EWidgetType::Permanent;

	WidgetZOrder = 100;
}

void UWidgetTransitionMaskBase::OnCreate(UObject* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnCreate(InOwner, InParams);
}

void UWidgetTransitionMaskBase::OnInitialize(UObject* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnInitialize(InOwner, InParams);
}

void UWidgetTransitionMaskBase::OnOpen(const TArray<FParameter>& InParams, bool bInstant)
{
	Super::OnOpen(InParams, bInstant);
}

void UWidgetTransitionMaskBase::OnClose(bool bInstant)
{
	Super::OnClose(bInstant);
}

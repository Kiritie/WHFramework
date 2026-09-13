// Fill out your copyright notice in the Description page of Project Settings.

#include "Animation/Widget/WidgetTransitionMaskBase.h"

UWidgetTransitionMaskBase::UWidgetTransitionMaskBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{

}

void UWidgetTransitionMaskBase::OnCreate(const FParameter& InParams)
{
	Super::OnCreate(InParams);
}

void UWidgetTransitionMaskBase::OnInitialize(const FParameter& InParams)
{
	Super::OnInitialize(InParams);
}

void UWidgetTransitionMaskBase::OnOpen(const FParameter& InParams, bool bInstant)
{
	Super::OnOpen(InParams, bInstant);
}

void UWidgetTransitionMaskBase::OnClose(bool bInstant)
{
	Super::OnClose(bInstant);
}

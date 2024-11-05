// Fill out your copyright notice in the Description page of Project Settings.

#include "Scene/Widget/WidgetWorldText.h"

UWidgetWorldText::UWidgetWorldText(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	WidgetName = FName("WorldText");
	WidgetInputMode = EInputMode::None;

	WidgetVisibility = EWorldWidgetVisibility::DistanceOnly;
	WidgetShowDistance = 5000.f;
}

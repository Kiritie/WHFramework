// Fill out your copyright notice in the Description page of Project Settings.


#include "Scene/Widget/WidgetWorldText.h"

UWidgetWorldText::UWidgetWorldText(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	WidgetName = FName("WorldText");
	WidgetInputMode = EInputMode::None;

	WidgetVisibilityMode = EWorldWidgetVisibilityMode::DistanceOnly;
	WidgetShowDistance = 1500.f;
}

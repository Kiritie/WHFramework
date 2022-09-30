// Fill out your copyright notice in the Description page of Project Settings.


#include "Scene/Widget/WidgetWorldText.h"

UWidgetWorldText::UWidgetWorldText(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	WidgetName = FName("WorldText");
	InputMode = EInputMode::None;

	bWidgetAutoVisibility = true;
	WidgetShowDistance = 1000.f;
}

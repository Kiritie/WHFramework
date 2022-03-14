// Fill out your copyright notice in the Description page of Project Settings.


#include "Widget/World/WorldWidgetContainer.h"

#include "Components/CanvasPanel.h"
#include "Widget/World/WorldWidgetBase.h"

UWorldWidgetContainer::UWorldWidgetContainer(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

UCanvasPanelSlot* UWorldWidgetContainer::AddWorldWidget_Implementation(UWorldWidgetBase* InWidget)
{
	if(UCanvasPanel* CanvasPanel = Cast<UCanvasPanel>(GetRootWidget()))
	{
		return CanvasPanel->AddChildToCanvas(InWidget);
	}
	return nullptr;
}

bool UWorldWidgetContainer::RemoveWorldWidget_Implementation(UWorldWidgetBase* InWidget)
{
	if(UCanvasPanel* CanvasPanel = Cast<UCanvasPanel>(GetRootWidget()))
	{
		return CanvasPanel->RemoveChild(InWidget);
	}
	return false;
}

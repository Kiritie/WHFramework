// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "Blueprint/UserWidget.h"

#include "WorldWidgetContainer.generated.h"

class UCanvasPanelSlot;
class UWorldWidgetBase;

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UWorldWidgetContainer : public UUserWidget
{
	GENERATED_BODY()

public:
	UWorldWidgetContainer(const FObjectInitializer& ObjectInitializer);

public:
	UFUNCTION(BlueprintNativeEvent)
	UCanvasPanelSlot* AddWorldWidget(UWorldWidgetBase* InWidget);

	UFUNCTION(BlueprintNativeEvent)
	bool RemoveWorldWidget(UWorldWidgetBase* InWidget);
};

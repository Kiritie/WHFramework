// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "WHFramework.h"

#include "Blueprint/UserWidget.h"

#include "WorldWidgetContainer.generated.h"

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
	class UCanvasPanelSlot* AddWorldWidget(class UWorldWidgetBase* InWidget);

	UFUNCTION(BlueprintNativeEvent)
	bool RemoveWorldWidget(class UWorldWidgetBase* InWidget);
};

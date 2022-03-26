// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Ability/AbilityModuleTypes.h"
#include "Blueprint/UserWidget.h"
#include "Widget/World/WorldWidgetBase.h"
#include "WidgetWorldText.generated.h"

/**
 * �����ı�
 */
UCLASS()
class DREAMWORLD_API UWidgetWorldText : public UWorldWidgetBase
{
	GENERATED_BODY()

public:
	UWidgetWorldText(const FObjectInitializer& ObjectInitializer);

public:
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable)
	void InitContent(const FString& InContent, EWorldTextType InContentType);
};

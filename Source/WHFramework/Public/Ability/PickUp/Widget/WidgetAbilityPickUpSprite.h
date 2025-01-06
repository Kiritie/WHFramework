// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Ability/AbilityModuleTypes.h"
#include "Widget/World/WorldWidgetBase.h"

#include "WidgetAbilityPickUpSprite.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UWidgetAbilityPickUpSprite : public UWorldWidgetBase
{
	GENERATED_BODY()

public:
	UWidgetAbilityPickUpSprite(const FObjectInitializer& ObjectInitializer);

public:
	UFUNCTION(BlueprintNativeEvent)
	void InitAbilityItem(const FAbilityItem& InAbilityItem);
};

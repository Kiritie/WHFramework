// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Ability/AbilityModuleTypes.h"
#include "Widget/World/WorldWidgetBase.h"

#include "WidgetAbilitySpawnerInfo.generated.h"

/**
 * ������Ѫ��
 */
UCLASS()
class WHFRAMEWORK_API UWidgetAbilitySpawnerInfo : public UWorldWidgetBase
{
	GENERATED_BODY()

public:
	UWidgetAbilitySpawnerInfo(const FObjectInitializer& ObjectInitializer);

public:
	UFUNCTION(BlueprintNativeEvent)
	void InitAbilityItem(const FAbilityItem& InAbilityItem);
};

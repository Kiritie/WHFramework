// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Blueprint/UserWidget.h"
#include "Scene/SceneModuleTypes.h"
#include "Widget/World/WorldWidgetBase.h"
#include "WidgetWorldText.generated.h"

/**
 * �����ı�
 */
UCLASS()
class WHFRAMEWORK_API UWidgetWorldText : public UWorldWidgetBase
{
	GENERATED_BODY()

public:
	UWidgetWorldText(const FObjectInitializer& ObjectInitializer);
};

// Fill out your copyright notice in the Description Item of Project Settings.

#pragma once
#include "WidgetSettingItemBase.h"

#include "WidgetInputSettingItemBase.generated.h"

class UEditableTextBox;
/**
 *
 */
UCLASS()
class WHFRAMEWORK_API UWidgetInputSettingItemBase : public UWidgetSettingItemBase
{
	GENERATED_BODY()

public:
	UWidgetInputSettingItemBase(const FObjectInitializer& ObjectInitializer);

public:
	virtual void NativeOnCurrentTextStyleChanged() override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (BindWidget, OptionalWidget = false), Category = "Components")
	UEditableTextBox* TxtBox_Value;
};

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "Setting/Widget/Item/WidgetBoolSettingItemBase.h"
#include "Setting/Widget/Page/WidgetSettingPageBase.h"

#include "WidgetVideoSettingPageBase.generated.h"

class UWidgetEnumSettingItemBase;

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UWidgetVideoSettingPageBase : public UWidgetSettingPageBase
{
	GENERATED_BODY()
	
public:
	UWidgetVideoSettingPageBase(const FObjectInitializer& ObjectInitializer);
	
public:
	virtual void OnCreate(UUserWidget* InOwner, const TArray<FParameter>& InParams) override;

	virtual void OnReset(bool bForce) override;

	virtual void OnApply() override;

	virtual void NativeOnActivated() override;

	virtual void NativeOnDeactivated() override;

	virtual void OnValueChange(UWidgetSettingItemBase* InSettingItem, const FParameter& InValue) override;

public:
	virtual bool CanApply_Implementation() const override;

	virtual bool CanReset_Implementation() const override;

protected:
	virtual FSaveData* GetDefaultSaveData() const override;

protected:
	UPROPERTY(BlueprintReadOnly, Category = "Components")
	UWidgetEnumSettingItemBase* SettingItem_WindowMode;

	UPROPERTY(BlueprintReadOnly, Category = "Components")
	UWidgetEnumSettingItemBase* SettingItem_WindowResolution;

	UPROPERTY(BlueprintReadOnly, Category = "Components")
	UWidgetBoolSettingItemBase* SettingItem_EnableVSync;

	UPROPERTY(BlueprintReadOnly, Category = "Components")
	UWidgetBoolSettingItemBase* SettingItem_EnableDynamicResolution;

	UPROPERTY(BlueprintReadOnly, Category = "Components")
	UWidgetEnumSettingItemBase* SettingItem_GlobalVideoQuality;

	UPROPERTY(BlueprintReadOnly, Category = "Components")
	UWidgetEnumSettingItemBase* SettingItem_ViewDistanceQuality;

	UPROPERTY(BlueprintReadOnly, Category = "Components")
	UWidgetEnumSettingItemBase* SettingItem_ShadowQuality;

	UPROPERTY(BlueprintReadOnly, Category = "Components")
	UWidgetEnumSettingItemBase* SettingItem_GlobalIlluminationQuality;

	UPROPERTY(BlueprintReadOnly, Category = "Components")
	UWidgetEnumSettingItemBase* SettingItem_ReflectionQuality;

	UPROPERTY(BlueprintReadOnly, Category = "Components")
	UWidgetEnumSettingItemBase* SettingItem_AntiAliasingQuality;

	UPROPERTY(BlueprintReadOnly, Category = "Components")
	UWidgetEnumSettingItemBase* SettingItem_TextureQuality;

	UPROPERTY(BlueprintReadOnly, Category = "Components")
	UWidgetEnumSettingItemBase* SettingItem_VisualEffectQuality;

	UPROPERTY(BlueprintReadOnly, Category = "Components")
	UWidgetEnumSettingItemBase* SettingItem_PostProcessingQuality;

	UPROPERTY(BlueprintReadOnly, Category = "Components")
	UWidgetEnumSettingItemBase* SettingItem_FoliageQuality;

	UPROPERTY(BlueprintReadOnly, Category = "Components")
	UWidgetEnumSettingItemBase* SettingItem_ShadingQuality;
};

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "WidgetSettingPageBase.h"
#include "Common/CommonTypes.h"

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
	virtual void OnInitialize(UObject* InOwner) override;

	virtual void OnCreate(UObject* InOwner) override;

	virtual void OnOpen(const TArray<FParameter>& InParams, bool bInstant) override;

	virtual void OnApply() override;

	virtual void OnReset() override;

	virtual void OnValueChange(UWidgetSettingItemBase* InSettingItem, const FParameter& InValue) override;

	virtual void OnClose(bool bInstant) override;

public:
	virtual bool CanApply_Implementation() const override;

	virtual bool CanReset_Implementation() const override;

protected:
	virtual FSaveData* GetDefaultSaveData() const override;

protected:
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

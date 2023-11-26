// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "WidgetSettingPageBase.h"
#include "Common/CommonTypes.h"

#include "WidgetAudioSettingPageBase.generated.h"

class UWidgetFloatSettingItemBase;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UWidgetAudioSettingPageBase : public UWidgetSettingPageBase
{
	GENERATED_BODY()
	
public:
	UWidgetAudioSettingPageBase(const FObjectInitializer& ObjectInitializer);
	
public:
	virtual void OnInitialize(UObject* InOwner, const TArray<FParameter>& InParams) override;

	virtual void OnCreate(UObject* InOwner, const TArray<FParameter>& InParams) override;

	virtual void OnOpen(const TArray<FParameter>& InParams, bool bInstant) override;

	virtual void OnApply() override;

	virtual void OnReset() override;

	virtual void OnClose(bool bInstant) override;

public:
	virtual bool CanApply_Implementation() const override;

	virtual bool CanReset_Implementation() const override;

protected:
	virtual FSaveData* GetDefaultSaveData() const override;

protected:
	UPROPERTY(BlueprintReadOnly, Category = "Components")
	UWidgetFloatSettingItemBase* SettingItem_GlobalSoundVolume;

	UPROPERTY(BlueprintReadOnly, Category = "Components")
	UWidgetFloatSettingItemBase* SettingItem_BackgroundSoundVolume;

	UPROPERTY(BlueprintReadOnly, Category = "Components")
	UWidgetFloatSettingItemBase* SettingItem_EnvironmentSoundVolume;

	UPROPERTY(BlueprintReadOnly, Category = "Components")
	UWidgetFloatSettingItemBase* SettingItem_EffectSoundVolume;
};

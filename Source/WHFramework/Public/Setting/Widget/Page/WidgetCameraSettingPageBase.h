// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "WidgetSettingPageBase.h"
#include "Common/CommonTypes.h"

#include "WidgetCameraSettingPageBase.generated.h"

class UWidgetBoolSettingItemBase;
class UWidgetFloatSettingItemBase;

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UWidgetCameraSettingPageBase : public UWidgetSettingPageBase
{
	GENERATED_BODY()
	
public:
	UWidgetCameraSettingPageBase(const FObjectInitializer& ObjectInitializer);
	
public:
	virtual void OnInitialize(UObject* InOwner) override;

	virtual void OnCreate(UObject* InOwner) override;

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
	UWidgetFloatSettingItemBase* SettingItem_CameraMoveRate;

	UPROPERTY(BlueprintReadOnly, Category = "Components")
	UWidgetBoolSettingItemBase* SettingItem_SmoothCameraMove;

	UPROPERTY(BlueprintReadOnly, Category = "Components")
	UWidgetFloatSettingItemBase* SettingItem_CameraMoveSpeed;

	UPROPERTY(BlueprintReadOnly, Category = "Components")
	UWidgetBoolSettingItemBase* SettingItem_ReverseCameraPitch;

	UPROPERTY(BlueprintReadOnly, Category = "Components")
	UWidgetFloatSettingItemBase* SettingItem_CameraTurnRate;

	UPROPERTY(BlueprintReadOnly, Category = "Components")
	UWidgetFloatSettingItemBase* SettingItem_CameraLookUpRate;

	UPROPERTY(BlueprintReadOnly, Category = "Components")
	UWidgetBoolSettingItemBase* SettingItem_SmoothCameraRotate;

	UPROPERTY(BlueprintReadOnly, Category = "Components")
	UWidgetFloatSettingItemBase* SettingItem_CameraRotateSpeed;

	UPROPERTY(BlueprintReadOnly, Category = "Components")
	UWidgetFloatSettingItemBase* SettingItem_CameraZoomRate;

	UPROPERTY(BlueprintReadOnly, Category = "Components")
	UWidgetBoolSettingItemBase* SettingItem_SmoothCameraZoom;

	UPROPERTY(BlueprintReadOnly, Category = "Components")
	UWidgetFloatSettingItemBase* SettingItem_CameraZoomSpeed;
};

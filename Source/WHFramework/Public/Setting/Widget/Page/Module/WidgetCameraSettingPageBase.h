// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "Setting/Widget/Page/WidgetSettingPageBase.h"

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
	virtual void OnInitialize(UObject* InOwner, const TArray<FParameter>& InParams) override;

	virtual void OnCreate(UObject* InOwner, const TArray<FParameter>& InParams) override;

	virtual void OnOpen(const TArray<FParameter>& InParams, bool bInstant) override;

	virtual void OnApply() override;

	virtual void OnReset(bool bForce) override;

	virtual void OnClose(bool bInstant) override;

protected:
	virtual void OnValueChange(UWidgetSettingItemBase* InSettingItem, const FParameter& InValue) override;

public:
	virtual bool CanApply_Implementation() const override;

	virtual bool CanReset_Implementation() const override;

protected:
	virtual FSaveData* GetDefaultSaveData() const override;

protected:
	UPROPERTY(BlueprintReadOnly, Category = "Components")
	UWidgetBoolSettingItemBase* SettingItem_ReverseCameraPanMove;

	UPROPERTY(BlueprintReadOnly, Category = "Components")
	UWidgetBoolSettingItemBase* SettingItem_EnableCameraPanZMove;

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

protected:
	UPROPERTY(EditDefaultsOnly)
	float MinCameraMoveRate;
	UPROPERTY(EditDefaultsOnly)
	float MaxCameraMoveRate;
	
	UPROPERTY(EditDefaultsOnly)
	float MinCameraMoveSpeed;
	UPROPERTY(EditDefaultsOnly)
	float MaxCameraMoveSpeed;

	UPROPERTY(EditDefaultsOnly)
	float MinCameraTurnRate;
	UPROPERTY(EditDefaultsOnly)
	float MaxCameraTurnRate;

	UPROPERTY(EditDefaultsOnly)
	float MinCameraLookUpRate;
	UPROPERTY(EditDefaultsOnly)
	float MaxCameraLookUpRate;

	UPROPERTY(EditDefaultsOnly)
	float MinCameraRotateSpeed;
	UPROPERTY(EditDefaultsOnly)
	float MaxCameraRotateSpeed;

	UPROPERTY(EditDefaultsOnly)
	float MinCameraZoomRate;
	UPROPERTY(EditDefaultsOnly)
	float MaxCameraZoomRate;

	UPROPERTY(EditDefaultsOnly)
	float MinCameraZoomSpeed;
	UPROPERTY(EditDefaultsOnly)
	float MaxCameraZoomSpeed;
};

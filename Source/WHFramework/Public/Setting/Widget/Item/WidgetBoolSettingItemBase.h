// Fill out your copyright notice in the Description Item of Project Settings.

#pragma once
#include "WidgetSettingItemBase.h"

#include "WidgetBoolSettingItemBase.generated.h"

class UCommonButton;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UWidgetBoolSettingItemBase : public UWidgetSettingItemBase
{
	GENERATED_BODY()
	
public:
	UWidgetBoolSettingItemBase(const FObjectInitializer& ObjectInitializer);

public:
	virtual void OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams) override;

	virtual void OnDespawn_Implementation(bool bRecovery) override;

public:
	virtual void OnRefresh() override;

protected:
	UFUNCTION()
	virtual void OnCheckBoxStateChanged(bool bIsChecked);

public:
	virtual FParameter GetValue() const override;

	virtual void SetValue(const FParameter& InValue) override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Components", meta = (BindWidget, OptionalWidget = false))
	UCommonButton* Btn_Value;
};

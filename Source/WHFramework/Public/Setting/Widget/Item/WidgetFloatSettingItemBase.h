// Fill out your copyright notice in the Description Item of Project Settings.

#pragma once
#include "WidgetSettingItemBase.h"

#include "WidgetFloatSettingItemBase.generated.h"

class USlider;
class UTextBlock;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UWidgetFloatSettingItemBase : public UWidgetSettingItemBase
{
	GENERATED_BODY()

public:
	UWidgetFloatSettingItemBase(const FObjectInitializer& ObjectInitializer);

public:
	virtual void OnSpawn_Implementation(const TArray<FParameter>& InParams) override;

	virtual void OnDespawn_Implementation(bool bRecovery) override;

public:
	virtual void OnCreate(UUserWidgetBase* InOwner, const TArray<FParameter>& InParams) override;

	virtual void OnInitialize(const TArray<FParameter>& InParams) override;

	virtual void OnRefresh() override;

	virtual void OnDestroy() override;

protected:
	UFUNCTION()
	virtual void OnSliderValueChanged(float InValue);

public:
	virtual FParameter GetValue() const override;

	virtual void SetValue(const FParameter& InValue) override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Components", meta = (BindWidget, OptionalWidget = false))
	USlider* Slider_Value;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Components", meta = (BindWidget, OptionalWidget = false))
	UTextBlock* Txt_MinValue;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Components", meta = (BindWidget, OptionalWidget = false))
	UTextBlock* Txt_MaxValue;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Components", meta = (BindWidget, OptionalWidget = false))
	UTextBlock* Txt_CurrentValue;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float MinValue;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float MaxValue;

private:
	int32 DecimalNum;
	float ScaleFactor;

public:
	float GetMinValue() const { return MinValue; }

	float GetMaxValue() const  { return MaxValue; }
};

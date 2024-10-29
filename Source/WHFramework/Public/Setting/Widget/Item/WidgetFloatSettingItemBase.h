// Fill out your copyright notice in the Description Item of Project Settings.

#pragma once
#include "WidgetSettingItemBase.h"

#include "WidgetFloatSettingItemBase.generated.h"

class UEditableTextBox;
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
	virtual void OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams) override;

	virtual void OnDespawn_Implementation(bool bRecovery) override;

public:
	virtual void OnRefresh() override;

protected:
	UFUNCTION()
	virtual void OnSliderValueChanged(float InValue);
	
	UFUNCTION()
	virtual void OnTextBoxValueCommitted(const FText& InText, ETextCommit::Type InCommitMethod);

public:
	virtual FParameter GetValue() const override;

	virtual void SetValue(const FParameter& InValue) override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (BindWidget, OptionalWidget = false), Category = "Components")
	USlider* Slider_Value;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (BindWidget, OptionalWidget = false), Category = "Components")
	UTextBlock* Txt_MinValue;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (BindWidget, OptionalWidget = false), Category = "Components")
	UTextBlock* Txt_MaxValue;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (BindWidget, OptionalWidget = false), Category = "Components")
	UTextBlock* Txt_Value;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (BindWidget, OptionalWidget = false), Category = "Components")
	UEditableTextBox* TxtBox_Value;

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

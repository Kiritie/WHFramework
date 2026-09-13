#pragma once

#include "Components/EditableTextBox.h"
#include "Setting/Widget/Entry/WidgetSettingEntryBase.h"

#include "WidgetNumberSettingEntryBase.generated.h"

class USlider;
class UTextBlock;

UCLASS()
class WHFRAMEWORK_API UWidgetNumberSettingEntryBase : public UWidgetSettingEntryBase
{
	GENERATED_BODY()

public:
	UWidgetNumberSettingEntryBase(const FObjectInitializer& ObjectInitializer);

	//////////////////////////////////////////////////////////////////////////
	/// ObjectPool
public:
	virtual void OnSpawn_Implementation(const FParameter& InParam) override;

	virtual void OnDespawn_Implementation(EObjectDespawnMode InMode) override;

protected:
	virtual void ApplyValueToControl_Implementation(const FParameter& InValue) override;

	UFUNCTION()
	void OnSliderValueChanged(float InValue);

	UFUNCTION()
	void OnTextValueCommitted(const FText& InText, ETextCommit::Type InCommitMethod);

	FParameter MakeTypedValue(double InValue) const;

	double GetNumericValue(const FParameter& InValue) const;

	FText FormatValue(double InValue) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (BindWidget, OptionalWidget = true), Category = "Components")
	TObjectPtr<USlider> Slider_Value;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (BindWidget, OptionalWidget = true), Category = "Components")
	TObjectPtr<UTextBlock> Txt_MinValue;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (BindWidget, OptionalWidget = true), Category = "Components")
	TObjectPtr<UTextBlock> Txt_MaxValue;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (BindWidget, OptionalWidget = true), Category = "Components")
	TObjectPtr<UTextBlock> Txt_Value;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (BindWidget, OptionalWidget = true), Category = "Components")
	TObjectPtr<UEditableTextBox> TxtBox_Value;
};

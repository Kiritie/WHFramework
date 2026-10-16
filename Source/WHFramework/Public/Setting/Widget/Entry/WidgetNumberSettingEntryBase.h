#pragma once

#include "Setting/Widget/Entry/WidgetSettingEntryBase.h"

#include "WidgetNumberSettingEntryBase.generated.h"

class UCommonRange;

UCLASS()
class WHFRAMEWORK_API UWidgetNumberSettingEntryBase : public UWidgetSettingEntryBase
{
	GENERATED_BODY()

public:
	UWidgetNumberSettingEntryBase(const FObjectInitializer& ObjectInitializer);

protected:
	virtual UWidget* GetValueControl() const override;
	virtual FName GetRendererName() const override;
	virtual void BindValueControl() override;
	virtual void UnbindValueControl() override;
	virtual void ApplyValueToControl_Implementation(const FParameter& InValue) override;
	virtual void ConfigureValueControl() override;

	UFUNCTION()
	void OnValueChanged(UCommonRange* InRange, const FParameter& InValue);

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Components")
	TObjectPtr<UCommonRange> Range_Value;
};

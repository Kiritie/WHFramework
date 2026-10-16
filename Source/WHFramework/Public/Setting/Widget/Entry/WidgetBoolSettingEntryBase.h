#pragma once

#include "Setting/Widget/Entry/WidgetSettingEntryBase.h"

#include "WidgetBoolSettingEntryBase.generated.h"

class UCommonToggle;

UCLASS()
class WHFRAMEWORK_API UWidgetBoolSettingEntryBase : public UWidgetSettingEntryBase
{
	GENERATED_BODY()

public:
	UWidgetBoolSettingEntryBase(const FObjectInitializer& ObjectInitializer);

protected:
	virtual UWidget* GetValueControl() const override;
	virtual FName GetRendererName() const override;
	virtual void BindValueControl() override;
	virtual void UnbindValueControl() override;
	virtual void ApplyValueToControl_Implementation(const FParameter& InValue) override;

	UFUNCTION()
	void OnValueChanged(UCommonToggle* InToggle, bool bInChecked);

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Components")
	TObjectPtr<UCommonToggle> Toggle_Value;
};

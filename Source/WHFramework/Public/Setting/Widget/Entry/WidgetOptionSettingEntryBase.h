#pragma once

#include "Setting/Widget/Entry/WidgetSettingEntryBase.h"

#include "WidgetOptionSettingEntryBase.generated.h"

class UCommonOptionSelector;

UCLASS()
class WHFRAMEWORK_API UWidgetOptionSettingEntryBase : public UWidgetSettingEntryBase
{
	GENERATED_BODY()

public:
	UWidgetOptionSettingEntryBase(const FObjectInitializer& ObjectInitializer);

protected:
	virtual UWidget* GetValueControl() const override;
	virtual FName GetRendererName() const override;
	virtual void BindValueControl() override;
	virtual void UnbindValueControl() override;
	virtual void ApplyValueToControl_Implementation(const FParameter& InValue) override;
	virtual void ConfigureValueControl() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	bool bUseDropdown = false;

	UFUNCTION()
	void OnOptionChanged(UCommonOptionSelector* InSelector, int32 InIndex);

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Components")
	TObjectPtr<UCommonOptionSelector> Selector_Value;
};

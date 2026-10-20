#pragma once

#include "Setting/Widget/Entry/WidgetSettingEntryBase.h"
#include "WidgetCustomSettingEntryBase.generated.h"

class UContentWidget;
class UWidgetCustomSettingValueBase;

UCLASS()
class WHFRAMEWORK_API UWidgetCustomSettingEntryBase : public UWidgetSettingEntryBase
{
	GENERATED_BODY()

public:
	virtual void SetSettingEntry(USettingEntry* InEntry) override;

protected:
	virtual UWidget* GetValueControl() const override;
	virtual FName GetRendererName() const override;
	virtual void ConfigureValueControl() override;
	virtual void BindValueControl() override;
	virtual void UnbindValueControl() override;
	virtual void OnModelStateChanged() override;
	virtual void ApplyValueToControl_Implementation(const FParameter& InValue) override;

	UFUNCTION()
	void OnValueChanged(UWidgetCustomSettingValueBase* InControl, const FParameter& InValue);

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UContentWidget> ValueHost;

	UPROPERTY(Transient)
	TObjectPtr<UWidgetCustomSettingValueBase> ValueControl;
};

#pragma once

#include "Setting/Widget/Entry/WidgetSettingEntryBase.h"

#include "WidgetKeySettingEntryBase.generated.h"

class UCommonButton;
class UWidgetPressAnyKeyPanelBase;

UCLASS()
class WHFRAMEWORK_API UWidgetKeySettingEntryBase : public UWidgetSettingEntryBase
{
	GENERATED_BODY()

public:
	UWidgetKeySettingEntryBase(const FObjectInitializer& ObjectInitializer);

	//////////////////////////////////////////////////////////////////////////
	/// ObjectPool
protected:
	virtual UWidget* GetValueControl() const override;
	virtual FName GetRendererName() const override;
	virtual void BindValueControl() override;
	virtual void UnbindValueControl() override;
	virtual void OnModelStateChanged() override;
	virtual void ApplyValueToControl_Implementation(const FParameter& InValue) override;

	void OnValueClicked();

	void OnKeySelected(FKey InKey, UWidgetPressAnyKeyPanelBase* InPanel);

	void OnKeySelectionCanceled(UWidgetPressAnyKeyPanelBase* InPanel);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (BindWidget, OptionalWidget = false), Category = "Components")
	TObjectPtr<UCommonButton> Btn_Value;

	TWeakObjectPtr<UWidgetPressAnyKeyPanelBase> CapturePanel;
	TWeakObjectPtr<USettingEntry> CaptureEntry;
	void ReleaseCapture(bool bClosePanel);
};

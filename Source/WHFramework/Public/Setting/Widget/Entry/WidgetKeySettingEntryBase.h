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
public:
	virtual void OnSpawn_Implementation(const FParameter& InParam) override;

	virtual void OnDespawn_Implementation(EObjectDespawnMode InMode) override;

protected:
	virtual void ApplyValueToControl_Implementation(const FParameter& InValue) override;

	void OnValueClicked();

	void OnKeySelected(FKey InKey, UWidgetPressAnyKeyPanelBase* InPanel);

	void OnKeySelectionCanceled(UWidgetPressAnyKeyPanelBase* InPanel);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (BindWidget, OptionalWidget = false), Category = "Components")
	TObjectPtr<UCommonButton> Btn_Value;
};

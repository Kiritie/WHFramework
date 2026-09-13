#pragma once

#include "Setting/Widget/Entry/WidgetSettingEntryBase.h"

#include "WidgetBoolSettingEntryBase.generated.h"

class UCommonButton;

UCLASS()
class WHFRAMEWORK_API UWidgetBoolSettingEntryBase : public UWidgetSettingEntryBase
{
	GENERATED_BODY()

public:
	UWidgetBoolSettingEntryBase(const FObjectInitializer& ObjectInitializer);

	//////////////////////////////////////////////////////////////////////////
	/// ObjectPool
public:
	virtual void OnSpawn_Implementation(const FParameter& InParam) override;

	virtual void OnDespawn_Implementation(EObjectDespawnMode InMode) override;

protected:
	virtual void ApplyValueToControl_Implementation(const FParameter& InValue) override;

	void OnValueChanged(bool bInSelected);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (BindWidget, OptionalWidget = false), Category = "Components")
	TObjectPtr<UCommonButton> Btn_Value;
};

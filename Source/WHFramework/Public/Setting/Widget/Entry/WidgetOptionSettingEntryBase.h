#pragma once

#include "Setting/Widget/Entry/WidgetSettingEntryBase.h"

#include "WidgetOptionSettingEntryBase.generated.h"

class UCommonButton;
class UEditableTextBox;

UCLASS()
class WHFRAMEWORK_API UWidgetOptionSettingEntryBase : public UWidgetSettingEntryBase
{
	GENERATED_BODY()

public:
	UWidgetOptionSettingEntryBase(const FObjectInitializer& ObjectInitializer);

	//////////////////////////////////////////////////////////////////////////
	/// ObjectPool
public:
	virtual void OnSpawn_Implementation(const FParameter& InParam) override;

	virtual void OnDespawn_Implementation(EObjectDespawnMode InMode) override;

protected:
	virtual void ApplyValueToControl_Implementation(const FParameter& InValue) override;

	void OnPreviousClicked();

	void OnNextClicked();

	int32 GetCurrentOptionIndex() const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (BindWidget, OptionalWidget = false), Category = "Components")
	TObjectPtr<UCommonButton> Btn_Last;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (BindWidget, OptionalWidget = false), Category = "Components")
	TObjectPtr<UCommonButton> Btn_Next;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (BindWidget, OptionalWidget = false), Category = "Components")
	TObjectPtr<UEditableTextBox> TxtBox_Value;
};

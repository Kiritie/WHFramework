#pragma once

#include "Setting/Widget/Entry/WidgetSettingEntryBase.h"

#include "WidgetEnumSettingEntryBase.generated.h"

class UComboBoxString;

UCLASS()
class WHFRAMEWORK_API UWidgetEnumSettingEntryBase : public UWidgetSettingEntryBase
{
	GENERATED_BODY()

public:
	UWidgetEnumSettingEntryBase(const FObjectInitializer& ObjectInitializer);

	//////////////////////////////////////////////////////////////////////////
	/// ObjectPool
public:
	virtual void OnSpawn_Implementation(const FParameter& InParam) override;

	virtual void OnDespawn_Implementation(EObjectDespawnMode InMode) override;

	virtual void SetSettingEntry(USettingEntry* InEntry) override;

protected:
	virtual void ApplyValueToControl_Implementation(const FParameter& InValue) override;

	UFUNCTION()
	void OnSelectionChanged(FString InSelectedItem, ESelectInfo::Type InSelectionType);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (BindWidget, OptionalWidget = false), Category = "Components")
	TObjectPtr<UComboBoxString> ComboBox_Value;
};

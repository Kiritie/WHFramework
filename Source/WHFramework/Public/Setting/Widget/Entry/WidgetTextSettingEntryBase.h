#pragma once

#include "Components/EditableTextBox.h"
#include "Setting/Widget/Entry/WidgetSettingEntryBase.h"

#include "WidgetTextSettingEntryBase.generated.h"

UCLASS()
class WHFRAMEWORK_API UWidgetTextSettingEntryBase : public UWidgetSettingEntryBase
{
	GENERATED_BODY()

public:
	UWidgetTextSettingEntryBase(const FObjectInitializer& ObjectInitializer);

	//////////////////////////////////////////////////////////////////////////
	/// ObjectPool
public:
	virtual void OnSpawn_Implementation(const FParameter& InParam) override;

	virtual void OnDespawn_Implementation(EObjectDespawnMode InMode) override;

protected:
	virtual void ApplyValueToControl_Implementation(const FParameter& InValue) override;

	UFUNCTION()
	void OnTextCommitted(const FText& InText, ETextCommit::Type InCommitMethod);

	FParameter MakeTypedValue(const FString& InText) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (BindWidget, OptionalWidget = false), Category = "Components")
	TObjectPtr<UEditableTextBox> TxtBox_Value;
};

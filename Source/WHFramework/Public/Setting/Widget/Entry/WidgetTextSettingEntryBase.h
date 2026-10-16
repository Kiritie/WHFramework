#pragma once

#include "Setting/Widget/Entry/WidgetSettingEntryBase.h"
#include "Types/SlateEnums.h"
#include "WidgetTextSettingEntryBase.generated.h"

class UCommonInput;

UCLASS()
class WHFRAMEWORK_API UWidgetTextSettingEntryBase : public UWidgetSettingEntryBase
{
	GENERATED_BODY()

public:
	UWidgetTextSettingEntryBase(const FObjectInitializer& ObjectInitializer);

protected:
	virtual UWidget* GetValueControl() const override;
	virtual FName GetRendererName() const override;
	virtual void BindValueControl() override;
	virtual void UnbindValueControl() override;
	virtual void ApplyValueToControl_Implementation(const FParameter& InValue) override;

	UFUNCTION()
	void OnTextCommitted(UCommonInput* InInput, const FText& InText, ETextCommit::Type InCommitMethod);

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Components")
	TObjectPtr<UCommonInput> Input_Value;
};

#pragma once

#include "Components/CheckBox.h"
#include "Widget/Common/CommonWidget.h"
#include "CommonToggle.generated.h"

class UCommonToggle;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCommonToggleChanged, UCommonToggle*, InToggle, bool, bInChecked);

UCLASS(BlueprintType, Blueprintable)
class WHFRAMEWORK_API UCommonToggle : public UCommonWidget
{
	GENERATED_BODY()

public:
	UCommonToggle(const FObjectInitializer& ObjectInitializer);
	virtual void OnDespawn_Implementation(EObjectDespawnMode InMode) override;

	UPROPERTY(BlueprintAssignable)
	FOnCommonToggleChanged OnValueChanged;

	UFUNCTION(BlueprintCallable)
	void SetCheckedState(ECheckBoxState InState, bool bNotify = false);

	UFUNCTION(BlueprintPure)
	ECheckBoxState GetCheckedState() const;

	UFUNCTION(BlueprintCallable)
	void SetIsChecked(bool bInChecked, bool bNotify = false);

	UFUNCTION(BlueprintPure)
	bool IsChecked() const;

protected:
	virtual EWidgetStyleKind GetStyleKind() const override;
	virtual void BuildNativeLayout() override;
	virtual void ApplyResolvedStyle(const UObject* InStyle) override;
	virtual void BindControlEvents() override;
	virtual void UnbindControlEvents() override;
	virtual void SynchronizeProperties() override;

	UFUNCTION()
	void HandleCheckChanged(bool bInChecked);

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UCheckBox> Chk_Value;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	ECheckBoxState CheckedState = ECheckBoxState::Unchecked;
};

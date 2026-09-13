#pragma once

#include "Parameter/ParameterTypes.h"
#include "Widget/Common/CommonButton.h"

#include "CommonTextInput.generated.h"

class UEditableTextBox;
class UCommonTextInput;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnCommonTextInputValueChanged,
	UCommonTextInput*,
	InInput,
	const FParameter&,
	InValue);

UCLASS()
class WHFRAMEWORK_API UCommonTextInput : public UCommonButton
{
	GENERATED_BODY()

public:
	UCommonTextInput(const FObjectInitializer& ObjectInitializer);

	//////////////////////////////////////////////////////////////////////////
	/// ObjectPool
public:
	virtual void OnSpawn_Implementation(const FParameter& InParam) override;

	virtual void OnDespawn_Implementation(EObjectDespawnMode InMode) override;

protected:
	UFUNCTION()
	void OnTextValueChanged(const FText& InText);

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (BindWidget, OptionalWidget = false), Category = "Components")
	TObjectPtr<UEditableTextBox> TxtBox_Value;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UScriptStruct> ParameterValueType;

public:
	UPROPERTY(BlueprintAssignable)
	FOnCommonTextInputValueChanged OnValueChanged;

	UFUNCTION(BlueprintPure)
	FParameter GetValue() const;

	UFUNCTION(BlueprintCallable)
	void SetValue(const FParameter& InValue);
};

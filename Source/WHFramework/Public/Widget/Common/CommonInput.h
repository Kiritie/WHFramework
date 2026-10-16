#pragma once

#include "Components/EditableTextBox.h"
#include "Widget/Common/CommonWidget.h"
#include "CommonInput.generated.h"

class UCommonInput;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCommonInputValueChanged, UCommonInput*, InInput, const FParameter&, InValue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnCommonInputTextCommitted, UCommonInput*, InInput, const FText&, InText, ETextCommit::Type, InCommitMethod);

UCLASS(BlueprintType, Blueprintable)
class WHFRAMEWORK_API UCommonInput : public UCommonWidget
{
	GENERATED_BODY()

public:
	UCommonInput(const FObjectInitializer& ObjectInitializer);
	virtual void OnDespawn_Implementation(EObjectDespawnMode InMode) override;

	UPROPERTY(BlueprintAssignable)
	FOnCommonInputValueChanged OnValueChanged;

	UPROPERTY(BlueprintAssignable)
	FOnCommonInputTextCommitted OnTextCommitted;

	UFUNCTION(BlueprintPure)
	virtual FParameter GetValue() const;

	UFUNCTION(BlueprintPure)
	bool TryGetValue(FParameter& OutValue) const;

	UFUNCTION(BlueprintCallable)
	virtual void SetValue(const FParameter& InValue);

	UFUNCTION(BlueprintPure)
	FText GetText() const;

	UFUNCTION(BlueprintCallable)
	void SetText(const FText& InText, bool bNotify = false);

	UFUNCTION(BlueprintCallable)
	void SetIsReadOnly(bool bInReadOnly);

	UFUNCTION(BlueprintCallable)
	void SetIsPassword(bool bInPassword);

	UFUNCTION(BlueprintCallable)
	void SetHintText(const FText& InHintText);

	UFUNCTION(BlueprintCallable)
	void SetUseStyleBackground(bool bInUseStyleBackground);

	UFUNCTION(BlueprintCallable)
	void SetInputFocus();

protected:
	virtual EWidgetStyleKind GetStyleKind() const override;
	virtual void BuildNativeLayout() override;
	virtual void ApplyResolvedStyle(const UObject* InStyle) override;
	virtual void BindControlEvents() override;
	virtual void UnbindControlEvents() override;
	virtual void SynchronizeProperties() override;

	UFUNCTION()
	virtual void HandleTextChanged(const FText& InText);

	UFUNCTION()
	void HandleTextCommitted(const FText& InText, ETextCommit::Type InCommitMethod);

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> TxtBox_Value;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText Text;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText HintText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bReadOnly = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bPassword = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bUseStyleBackground = true;

	UPROPERTY(Transient)
	TObjectPtr<UScriptStruct> ParameterValueType;
};

#pragma once

#include "Parameter/ParameterTypes.h"
#include "Widget/Common/CommonButton.h"

#include "CommonOptionSelector.generated.h"

class UEditableTextBox;
class UCommonOptionSelector;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnOptionSelectorValueChanged,
	UCommonOptionSelector*,
	InSelector,
	const FParameter&,
	InValue);

UCLASS()
class WHFRAMEWORK_API UCommonOptionSelector : public UCommonButton
{
	GENERATED_BODY()

public:
	UCommonOptionSelector(const FObjectInitializer& ObjectInitializer);

	//////////////////////////////////////////////////////////////////////////
	/// ObjectPool
public:
	virtual void OnSpawn_Implementation(const FParameter& InParam) override;

	virtual void OnDespawn_Implementation(EObjectDespawnMode InMode) override;

protected:
	UFUNCTION()
	void OnTextValueChanged(const FText& InText);

	void OnPreviousClicked();

	void OnNextClicked();

	void RefreshValue();

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (BindWidget, OptionalWidget = false), Category = "Components")
	TObjectPtr<UCommonButton> Btn_Last;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (BindWidget, OptionalWidget = false), Category = "Components")
	TObjectPtr<UCommonButton> Btn_Next;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (BindWidget, OptionalWidget = false), Category = "Components")
	TObjectPtr<UEditableTextBox> TxtBox_Value;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Option")
	TArray<FString> OptionNames;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Option")
	bool bEditable = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Option")
	int32 OptionIndex = 0;

public:
	UPROPERTY(BlueprintAssignable)
	FOnOptionSelectorValueChanged OnValueChanged;

	UFUNCTION(BlueprintPure)
	const TArray<FString>& GetOptionNames() const { return OptionNames; }

	UFUNCTION(BlueprintCallable)
	void SetOptionNames(const TArray<FString>& InOptionNames);

	UFUNCTION(BlueprintPure)
	int32 GetOptionIndex() const { return OptionIndex; }

	UFUNCTION(BlueprintPure)
	FString GetSelectedOption() const;

	UFUNCTION(BlueprintPure)
	FParameter GetValue() const;

	UFUNCTION(BlueprintCallable)
	void SetValue(const FParameter& InValue);

	UFUNCTION(BlueprintCallable)
	void SetOptionIndex(int32 InOptionIndex);
};

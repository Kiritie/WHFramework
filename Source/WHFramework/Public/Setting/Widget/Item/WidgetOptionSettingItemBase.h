// Fill out your copyright notice in the Description Item of Project Settings.

#pragma once
#include "WidgetSettingItemBase.h"
#include "Components/EditableTextBox.h"

#include "WidgetOptionSettingItemBase.generated.h"

class UComboBoxString;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UWidgetOptionSettingItemBase : public UWidgetSettingItemBase
{
	GENERATED_BODY()
	
public:
	UWidgetOptionSettingItemBase(const FObjectInitializer& ObjectInitializer);

public:
	virtual void OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams) override;

	virtual void OnDespawn_Implementation(bool bRecovery) override;

public:
	virtual void OnRefresh() override;

protected:
	UFUNCTION()
	virtual void OnTextBoxValueChanged(const FText& InText);

	UFUNCTION()
	virtual void OnLastButtonClicked();

	UFUNCTION()
	virtual void OnNextButtonClicked();

public:
	virtual FParameter GetValue() const override;

	virtual void SetValue(const FParameter& InValue) override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (BindWidget, OptionalWidget = false), Category = "Components")
	UEditableTextBox* TxtBox_Value;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (BindWidget, OptionalWidget = false), Category = "Components")
	UCommonButton* Btn_Last;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (BindWidget, OptionalWidget = false), Category = "Components")
	UCommonButton* Btn_Next;

	UPROPERTY()
	TArray<FString> OptionNames;

public:
	UFUNCTION(BlueprintPure)
	TArray<FString> GetOptionNames() const { return OptionNames; }

	UFUNCTION(BlueprintCallable)
	void SetOptionNames(const TArray<FString>& InOptionNames);
};

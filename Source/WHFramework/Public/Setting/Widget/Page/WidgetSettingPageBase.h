// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "Widget/Screen/UMG/UserWidgetBase.h"

#include "WidgetSettingPageBase.generated.h"

class UVerticalBox;
class UCommonButtonStyle;
class UWidgetSettingItemBase;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UWidgetSettingPageBase : public UUserWidgetBase
{
	GENERATED_BODY()
	
public:
	UWidgetSettingPageBase(const FObjectInitializer& ObjectInitializer);
	
public:
	virtual void OnInitialize(UObject* InOwner, const TArray<FParameter>& InParams) override;

	virtual void OnCreate(UObject* InOwner, const TArray<FParameter>& InParams) override;

	virtual void OnOpen(const TArray<FParameter>& InParams, bool bInstant) override;

	virtual void OnReset(bool bForce) override;

	virtual void OnClose(bool bInstant) override;

public:
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnApply")
	void K2_OnApply();
	UFUNCTION()
	virtual void OnApply();

protected:
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnValueChange")
	void K2_OnValueChange(UWidgetSettingItemBase* InSettingItem, const FParameter& InValue);
	UFUNCTION()
	virtual void OnValueChange(UWidgetSettingItemBase* InSettingItem, const FParameter& InValue);

	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnValuesChange")
	void K2_OnValuesChange(UWidgetSettingItemBase* InSettingItem, const TArray<FParameter>& InValues);
	UFUNCTION()
	virtual void OnValuesChange(UWidgetSettingItemBase* InSettingItem, const TArray<FParameter>& InValues);

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintPure)
	bool CanApply() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintPure)
	bool CanReset() const;

public:
	UFUNCTION(BlueprintCallable)
	void Apply();

public:
	UFUNCTION(BlueprintNativeEvent)
	void AddSettingItem(const FName InName, UWidgetSettingItemBase* InSettingItem, const FText& InCategory = FText::GetEmpty());

	UFUNCTION(BlueprintNativeEvent)
	void ClearSettingItems();

protected:
	virtual FSaveData* GetDefaultSaveData() const;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Components", meta = (BindWidget, OptionalWidget = false))
	UVerticalBox* ContentBox;

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<UCommonButtonStyle> PageItemStyle;

protected:
	FText LastCategory;

	UPROPERTY()
	TMap<FName, UWidgetSettingItemBase*> SettingItems;

public:
	UFUNCTION(BlueprintPure)
	UWidgetSettingItemBase* GetSettingItemByName(const FName InName);
};

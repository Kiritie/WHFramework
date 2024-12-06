// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "Widget/Sub/SubActivatableWidgetBase.h"

#include "WidgetSettingPageBase.generated.h"

class UVerticalBox;
class UCommonButtonStyle;
class UWidgetSettingItemBase;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UWidgetSettingPageBase : public USubActivatableWidgetBase
{
	GENERATED_BODY()
	
public:
	UWidgetSettingPageBase(const FObjectInitializer& ObjectInitializer);
	
public:
	virtual void OnCreate(UUserWidget* InOwner, const TArray<FParameter>& InParams) override;

	virtual void OnReset(bool bForce) override;

public:
	virtual void NativeOnActivated() override;

	virtual void NativeOnDeactivated() override;

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
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (BindWidget, OptionalWidget = false), Category = "Components")
	UVerticalBox* ContentBox;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<UCommonButtonStyle> PageItemStyle;

	FText LastCategory;

	UPROPERTY()
	TMap<FName, UWidgetSettingItemBase*> SettingItems;

public:
	FText GetTitle() const { return Title; }

	TSubclassOf<UCommonButtonStyle> GetPageItemStyle() const { return PageItemStyle; }

	UFUNCTION(BlueprintPure)
	UWidgetSettingItemBase* GetSettingItemByName(const FName InName);
};

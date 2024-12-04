// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "Setting/Widget/Page/WidgetSettingPageBase.h"

#include "WidgetInputSettingPageBase.generated.h"

class UWidgetKeySettingItemBase;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UWidgetInputSettingPageBase : public UWidgetSettingPageBase
{
	GENERATED_BODY()
	
public:
	UWidgetInputSettingPageBase(const FObjectInitializer& ObjectInitializer);
	
public:
	virtual void OnCreate(UUserWidget* InOwner, const TArray<FParameter>& InParams) override;

	virtual void OnReset(bool bForce) override;

	virtual void OnApply() override;

	virtual void OnActivated() override;

	virtual void OnDeactivated() override;

public:
	virtual bool CanApply_Implementation() const override;

	virtual bool CanReset_Implementation() const override;

protected:
	UFUNCTION(BlueprintNativeEvent)
	void AddShortcutSettingItem(const FName InName, UWidgetSettingItemBase* InSettingItem, const FText& InCategory = FText::GetEmpty());
	
	UFUNCTION(BlueprintNativeEvent)
	void AddMappingSettingItem(const FName InName, UWidgetSettingItemBase* InSettingItem, const FText& InCategory = FText::GetEmpty());
	
	virtual void ClearSettingItems_Implementation() override;

protected:
	virtual FSaveData* GetDefaultSaveData() const override;

protected:
	UPROPERTY()
	TMap<FName, UWidgetSettingItemBase*> ShortcutSettingItems;

	UPROPERTY()
	TMap<FName, UWidgetSettingItemBase*> MappingSettingItems;
};

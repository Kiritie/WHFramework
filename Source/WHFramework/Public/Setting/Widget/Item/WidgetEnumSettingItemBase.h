// Fill out your copyright notice in the Description Item of Project Settings.

#pragma once
#include "WidgetSettingItemBase.h"

#include "WidgetEnumSettingItemBase.generated.h"

class UComboBoxString;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UWidgetEnumSettingItemBase : public UWidgetSettingItemBase
{
	GENERATED_BODY()
	
public:
	UWidgetEnumSettingItemBase(const FObjectInitializer& ObjectInitializer);

public:
	virtual void OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams) override;

	virtual void OnDespawn_Implementation(bool bRecovery) override;

public:
	virtual void OnCreate(UUserWidgetBase* InOwner, const TArray<FParameter>& InParams) override;

	virtual void OnInitialize(const TArray<FParameter>& InParams) override;

	virtual void OnRefresh() override;

	virtual void OnDestroy(bool bRecovery) override;

protected:
	UFUNCTION()
	virtual void OnComboBoxSelectionChanged(FString InSelectedItem, ESelectInfo::Type InSelectionType);

public:
	virtual FParameter GetValue() const override;

	virtual void SetValue(const FParameter& InValue) override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Components", meta = (BindWidget, OptionalWidget = false))
	UComboBoxString* ComboBox_Value;
};

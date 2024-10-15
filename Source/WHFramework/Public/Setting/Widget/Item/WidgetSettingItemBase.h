// Fill out your copyright notice in the Description Item of Project Settings.

#pragma once
#include "Widget/Common/CommonButton.h"

#include "WidgetSettingItemBase.generated.h"

class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSettingItemValueChanged, UWidgetSettingItemBase*, InSettingItem, const FParameter&, InValue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSettingItemValuesChanged, UWidgetSettingItemBase*, InSettingItem, const TArray<FParameter>&, InValues);

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UWidgetSettingItemBase : public UCommonButton
{
	GENERATED_BODY()
	
public:
	UWidgetSettingItemBase(const FObjectInitializer& ObjectInitializer);

public:
	virtual int32 GetLimit_Implementation() const override { return -1; }

	virtual void OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams) override;

	virtual void OnDespawn_Implementation(bool bRecovery) override;

public:
	UFUNCTION()
	virtual void OnRefresh();

public:
	UFUNCTION()
	void Refresh();

protected:
	UPROPERTY(BlueprintReadOnly)
	FName SettingName;

public:
	UPROPERTY(BlueprintAssignable)
	FOnSettingItemValueChanged OnValueChanged;

	UPROPERTY(BlueprintAssignable)
	FOnSettingItemValuesChanged OnValuesChanged;

public:
	UFUNCTION(BlueprintPure)
	FName GetNameS() const { return SettingName; }

	UFUNCTION(BlueprintCallable)
	void SetNameS(const FName InName) { SettingName = InName; }

public:
	UFUNCTION(BlueprintPure)
	virtual FParameter GetValue() const;

	UFUNCTION(BlueprintCallable)
	virtual void SetValue(const FParameter& InValue);

	UFUNCTION(BlueprintPure)
	virtual TArray<FParameter> GetValues() const;

	UFUNCTION(BlueprintCallable)
	virtual void SetValues(const TArray<FParameter>& InValues);
};

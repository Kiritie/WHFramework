// Fill out your copyright notice in the Description Item of Project Settings.

#pragma once
#include "WidgetInputSettingItemBase.h"

#include "WidgetTextSettingItemBase.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UWidgetTextSettingItemBase : public UWidgetInputSettingItemBase
{
	GENERATED_BODY()
	
public:
	UWidgetTextSettingItemBase(const FObjectInitializer& ObjectInitializer);

public:
	virtual void OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams) override;

	virtual void OnDespawn_Implementation(bool bRecovery) override;

public:
	virtual void OnRefresh() override;

protected:
	UFUNCTION()
	virtual void OnTextBoxValueChanged(const FText& InText);

public:
	virtual FParameter GetValue() const override;

	virtual void SetValue(const FParameter& InValue) override;

protected:
	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<UScriptStruct> ParameterValueType;
};

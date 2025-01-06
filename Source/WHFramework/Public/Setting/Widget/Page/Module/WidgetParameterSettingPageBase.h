// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "Setting/Widget/Page/WidgetSettingPageBase.h"

#include "WidgetParameterSettingPageBase.generated.h"

class UWidgetKeySettingItemBase;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UWidgetParameterSettingPageBase : public UWidgetSettingPageBase
{
	GENERATED_BODY()
	
public:
	UWidgetParameterSettingPageBase(const FObjectInitializer& ObjectInitializer);
	
public:
	virtual void OnCreate(UUserWidget* InOwner, const TArray<FParameter>& InParams) override;

	virtual void OnReset(bool bForce) override;

	virtual void OnApply() override;

	virtual void NativeOnActivated() override;

	virtual void NativeOnDeactivated() override;

public:
	virtual bool CanApply_Implementation() const override;

	virtual bool CanReset_Implementation() const override;

protected:
	virtual FSaveData* GetDefaultSaveData() const override;
};

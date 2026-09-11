// Fill out your copyright notice in the Description Item of Project Settings.

#pragma once
#include "WidgetSettingItemBase.h"

#include "WidgetKeySettingItemBase.generated.h"

class UCommonButton;
class UEditableTextBox;
class UWidgetPressAnyKeyPanelBase;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UWidgetKeySettingItemBase : public UWidgetSettingItemBase
{
	GENERATED_BODY()
	
public:
	UWidgetKeySettingItemBase(const FObjectInitializer& ObjectInitializer);

public:
	virtual void OnSpawn_Implementation(
		const FParameter& InParameter) override;

	virtual void OnDespawn_Implementation(EObjectDespawnMode InMode)
		override;

public:
	virtual void OnRefresh() override;
	
protected:
	void OnValueButtonClicked(int32 InIndex);

	void OnKeySelected(FKey InKey, UWidgetPressAnyKeyPanelBase* InPressAnyKeyPanel);

	void OnKeySelectionCanceled(UWidgetPressAnyKeyPanelBase* InPressAnyKeyPanel);

public:
	virtual TArray<FParameter> GetValues() const override;

	virtual void SetValues(const TArray<FParameter>& InValues) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Components")
	TArray<UCommonButton*> Btn_Values;

	UPROPERTY(BlueprintReadOnly)
	TArray<FKey> Keys;
};

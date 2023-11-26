// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "Widget/Screen/UMG/UserWidgetBase.h"

#include "WidgetSettingPanelBase.generated.h"

class UCommonButton;
class UTextBlock;
/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UWidgetSettingPanelBase : public UUserWidgetBase
{
	GENERATED_BODY()
	
public:
	UWidgetSettingPanelBase(const FObjectInitializer& ObjectInitializer);
	
public:
	virtual void OnInitialize(UObject* InOwner, const TArray<FParameter>& InParams) override;

	virtual void OnOpen(const TArray<FParameter>& InParams, bool bInstant) override;

	virtual void OnClose(bool bInstant) override;

public:
	UFUNCTION(BlueprintCallable)
	void ChangePage(int32 InPageIndex);

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Components", meta = (BindWidget, OptionalWidget = false))
	UTextBlock* Txt_Title;

public:
	UPROPERTY(BlueprintReadOnly)
	int32 PageIndex;
};

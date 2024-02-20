// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Widget/Screen/UMG/SubWidgetBase.h"
#include "WidgetKeyTipsItemBase.generated.h"

class UBorder;
class UHorizontalBox;
class UCommonTextBlockN;

UCLASS(BlueprintType)
class WHFRAMEWORK_API UWidgetKeyTipsItemBase : public USubWidgetBase
{
	GENERATED_BODY()
	
public:
	UWidgetKeyTipsItemBase(const FObjectInitializer& ObjectInitializer);

public:
	virtual void OnCreate(UUserWidgetBase* InOwner, const TArray<FParameter>& InParams) override;

	virtual void OnInitialize(const TArray<FParameter>& InParams) override;

	virtual void OnRefresh() override;

	virtual void OnDestroy(bool bRecovery) override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Components", meta = (BindWidget, OptionalWidget = false))
	UHorizontalBox* Box_KeyIcon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Components", meta = (BindWidget, OptionalWidget = false))
	UBorder* Border_KeyCode;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Components", meta = (BindWidget, OptionalWidget = false))
	UCommonTextBlockN* Txt_KeyCode;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "KeyMapping")
	FString KeyMappingName;
};

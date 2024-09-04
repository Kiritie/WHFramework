// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "Widget/Screen/UserWidgetBase.h"

#include "WidgetSceneMaskBase.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UWidgetSceneMaskBase : public UUserWidgetBase
{
	GENERATED_BODY()
	
public:
	UWidgetSceneMaskBase(const FObjectInitializer& ObjectInitializer);
	
public:
	virtual void OnCreate(UObject* InOwner, const TArray<FParameter>& InParams) override;
	
	virtual void OnInitialize(UObject* InOwner, const TArray<FParameter>& InParams) override;

	virtual void OnOpen(const TArray<FParameter>& InParams, bool bInstant) override;

	virtual void OnClose(bool bInstant) override;

protected:
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

protected:
	FSimpleDynamicDelegate OnMaskClosed;
};

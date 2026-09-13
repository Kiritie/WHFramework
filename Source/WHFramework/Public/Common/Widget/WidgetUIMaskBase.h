// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "Widget/Screen/UserWidgetBase.h"

#include "WidgetUIMaskBase.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UWidgetUIMaskBase : public UUserWidgetBase
{
	GENERATED_BODY()
	
public:
	UWidgetUIMaskBase(const FObjectInitializer& ObjectInitializer);
	
public:
	virtual void OnCreate(const FParameter& InParam) override;
	
	virtual void OnInitialize(const FParameter& InParam) override;

	virtual void OnOpen(const FParameter& InParam, bool bInstant) override;

	virtual void OnClose(bool bInstant) override;

protected:
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

protected:
	FSimpleDynamicDelegate OnPressAnyKey;
};

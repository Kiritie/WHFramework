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
	virtual void OnCreate(UObject* InOwner, const TArray<FParameter>& InParams) override;
	
	virtual void OnInitialize(UObject* InOwner, const TArray<FParameter>& InParams) override;

	virtual void OnOpen(const TArray<FParameter>& InParams, bool bInstant) override;

	virtual void OnClose(bool bInstant) override;
};

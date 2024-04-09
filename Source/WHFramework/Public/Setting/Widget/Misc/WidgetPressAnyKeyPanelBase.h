// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widget/Screen/UserWidgetBase.h"

#include "WidgetPressAnyKeyPanelBase.generated.h"

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UWidgetPressAnyKeyPanelBase : public UUserWidgetBase
{
	GENERATED_BODY()

public:
	UWidgetPressAnyKeyPanelBase(const FObjectInitializer& Initializer);
	
public:
	virtual void OnInitialize(UObject* InOwner, const TArray<FParameter>& InParams) override;

	virtual void OnOpen(const TArray<FParameter>& InParams, bool bInstant) override;

	virtual void OnClose(bool bInstant) override;

protected:
	void HandleKeySelected(FKey InKey);
	
	void HandleKeySelectionCanceled();

	void Dismiss(TFunction<void()> PostDismissCallback);

public:
	DECLARE_EVENT_OneParam(UWidgetPressAnyKeyPanelBase, FOnKeySelected, FKey);
	FOnKeySelected OnKeySelected;
	
	DECLARE_EVENT(UWidgetPressAnyKeyPanelBase, FOnKeySelectionCanceled);
	FOnKeySelectionCanceled OnKeySelectionCanceled;

private:
	bool bKeySelected = false;
	TSharedPtr<class FPressAnyKeyInputProcessor> InputProcessor;
};

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Slate/Runtime/Base/SSlateWidgetBase.h"

/**
 * 
 */
class WHFRAMEWORK_API SDebugPanelWidget : public SSlateWidgetBase
{
public:
	SDebugPanelWidget();
	
	SLATE_BEGIN_ARGS(SDebugPanelWidget) {}
	
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

public:
	static FName WidgetName;
	
	static FName ParentName;

public:
	virtual void OnCreate(UObject* InOwner, const TArray<FParameter>& InParams) override;

	virtual void OnOpen(const TArray<FParameter>& InParams, bool bInstant) override;

	virtual void OnClose(bool bInstant) override;

	virtual FName GetParentName() const override { return ParentName; }
};

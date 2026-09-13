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

	static const FName WidgetName;
	
	static const FName ParentName;

public:
	virtual void OnCreate(const FParameter& InParams) override;

	virtual void OnOpen(const FParameter& InParams, bool bInstant) override;

	virtual void OnClose(bool bInstant) override;
};

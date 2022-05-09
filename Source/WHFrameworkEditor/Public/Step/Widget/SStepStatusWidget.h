// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Widget/SEditorSlateWidgetBase.h"

/**
 * 
 */
class WHFRAMEWORKEDITOR_API SStepStatusWidget : public SEditorSlateWidgetBase
{
public:
	SStepStatusWidget();

	SLATE_BEGIN_ARGS(SStepStatusWidget) {}

		SLATE_ARGUMENT(TSharedPtr<class SStepListWidget>, ListWidget)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

public:
	virtual void OnCreate() override;

	virtual void OnReset() override;

	virtual void OnRefresh() override;

	virtual void OnDestroy() override;

	//////////////////////////////////////////////////////////////////////////
	/// Refs
public:
	TSharedPtr<class SStepListWidget> ListWidget;
};

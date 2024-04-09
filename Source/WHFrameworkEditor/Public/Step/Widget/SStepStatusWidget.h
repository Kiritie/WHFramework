// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Step/StepEditor.h"
#include "Slate/Editor/Base/SEditorWidgetBase.h"

/**
 * 
 */
class WHFRAMEWORKEDITOR_API SStepStatusWidget : public SEditorWidgetBase
{
public:
	SStepStatusWidget();

	SLATE_BEGIN_ARGS(SStepStatusWidget) {}

		SLATE_ARGUMENT(TWeakPtr<FStepEditor>, StepEditor)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

public:
	static FName WidgetName;

public:
	virtual void OnCreate() override;

	virtual void OnReset() override;

	virtual void OnRefresh() override;

	virtual void OnDestroy() override;

	//////////////////////////////////////////////////////////////////////////
	/// Stats
public:
	TWeakPtr<FStepEditor> StepEditor;
};

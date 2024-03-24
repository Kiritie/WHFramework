// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Step/StepEditor.h"

#include "Base/SEditorWidgetBase.h"

class SStepListWidget;
class UStepModule;
/**
 * 
 */
class WHFRAMEWORKEDITOR_API SStepDetailsWidget : public SEditorWidgetBase
{
public:
	SStepDetailsWidget();
	
	SLATE_BEGIN_ARGS(SStepDetailsWidget) {}

		SLATE_ARGUMENT(TWeakPtr<FStepEditor>, StepEditor)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

public:
	virtual void OnCreate() override;

	virtual void OnReset() override;

	virtual void OnRefresh() override;

	virtual void OnDestroy() override;

	//////////////////////////////////////////////////////////////////////////
	/// Stats
public:
	TWeakPtr<FStepEditor> StepEditor;

	//////////////////////////////////////////////////////////////////////////
	/// Widgets
public:
	TSharedPtr<IDetailsView> DetailsView;
};

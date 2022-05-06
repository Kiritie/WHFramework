// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Step/Base/StepBase.h"
#include "Widget/SEditorSlateWidgetBase.h"

class SStepListWidget;
class AStepModule;
/**
 * 
 */
class WHFRAMEWORKEDITOR_API SStepDetailWidget : public SEditorSlateWidgetBase
{
public:
	SLATE_BEGIN_ARGS(SStepDetailWidget) {}

		SLATE_ARGUMENT(AStepModule*, StepModule)

		SLATE_ARGUMENT(TSharedPtr<SStepListWidget>, ListWidget)

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
	TArray<TSharedPtr<FStepListItem>> SelectedStepListItems;

	//////////////////////////////////////////////////////////////////////////
	/// Refs
public:
	AStepModule* StepModule;

	TSharedPtr<SStepListWidget> ListWidget;

	//////////////////////////////////////////////////////////////////////////
	/// Widgets
public:
	TSharedPtr<IDetailsView> DetailsView;

private:
	void UpdateDetailsView();

	void OnSelectStepListItem(TArray<TSharedPtr<FStepListItem>> StepListItem);
};

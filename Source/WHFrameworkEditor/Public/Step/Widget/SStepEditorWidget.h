// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Widget/SEditorSlateWidgetBase.h"

/**
 * 
 */
class WHFRAMEWORKEDITOR_API SStepEditorWidget : public SEditorSlateWidgetBase
{
public:
	SLATE_BEGIN_ARGS(SStepEditorWidget) {}
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
	bool bPreviewMode = false;
	bool bShowListPanel = true;
	bool bShowDetailPanel = true;
	bool bShowStatusPanel = true;

	//////////////////////////////////////////////////////////////////////////
	/// Refs
public:
	class AStepModule* StepModule;

	//////////////////////////////////////////////////////////////////////////
	/// Widgets
protected:
	TSharedPtr<class SStepToolbarWidget> ToolbarWidget;

	TSharedPtr<class SStepListWidget> ListWidget;

	TSharedPtr<class SStepDetailWidget> DetailWidget;

	TSharedPtr<class SStepStatusWidget> StatusWidget;

private:
	FDelegateHandle RefreshDelegateHandle;
	FDelegateHandle BeginPIEDelegateHandle;
	FDelegateHandle EndPIEDelegateHandle;

public:
	void TogglePreviewMode();

	void OnBeginPIE(bool bIsSimulating);

	void OnEndPIE(bool bIsSimulating);

	void SetIsPreviewMode(bool bIsPreviewMode);
};

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Widget/SEditorSlateWidgetBase.h"

/**
 * 
 */
class WHFRAMEWORKEDITOR_API SProcedureEditorWidget : public SEditorSlateWidgetBase
{
public:
	SLATE_BEGIN_ARGS(SProcedureEditorWidget) {}
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
	class AProcedureModule* ProcedureModule;

	//////////////////////////////////////////////////////////////////////////
	/// Widgets
public:
	TSharedPtr<class SProcedureToolbarWidget> ToolbarWidget;

	TSharedPtr<class SProcedureListWidget> ListWidget;

	TSharedPtr<class SProcedureDetailWidget> DetailWidget;

	TSharedPtr<class SProcedureStatusWidget> StatusWidget;

private:
	FDelegateHandle OnBeginPIEHandle;
	FDelegateHandle OnEndPIEHandle;
	FDelegateHandle OnOpenLevelHandle;
	FDelegateHandle OnBlueprintCompiledHandle;

public:
	void TogglePreviewMode();

	void OnBeginPIE(bool bIsSimulating);

	void OnEndPIE(bool bIsSimulating);

	void SetIsPreviewMode(bool bIsPreviewMode);
};

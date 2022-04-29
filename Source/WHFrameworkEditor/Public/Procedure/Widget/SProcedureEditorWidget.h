// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Widgets/SCompoundWidget.h"

/**
 * 
 */
class WHFRAMEWORKEDITOR_API SProcedureEditorWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SProcedureEditorWidget)
	{}
	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);

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
protected:
	TSharedPtr<class SProcedureToolbarWidget> ToolbarWidget;

	TSharedPtr<class SProcedureListWidget> ListWidget;

	TSharedPtr<class SProcedureDetailWidget> DetailWidget;

	TSharedPtr<class SProcedureStatusWidget> StatusWidget;

private:
	FDelegateHandle BeginPIEDelegateHandle;
	FDelegateHandle EndPIEDelegateHandle;

public:
	void TogglePreviewMode();

	void OnBeginPIE(bool bIsSimulating);

	void OnEndPIE(bool bIsSimulating);

	void SetIsPreviewMode(bool bIsPreviewMode);
};

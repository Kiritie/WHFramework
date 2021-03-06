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
	SProcedureEditorWidget();
	
	SLATE_BEGIN_ARGS(SProcedureEditorWidget) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

public:
	virtual void OnCreate() override;

	virtual void OnReset() override;

	virtual void OnRefresh() override;

	virtual void OnDestroy() override;

protected:
	void OnBeginPIE(bool bIsSimulating);

	void OnEndPIE(bool bIsSimulating);

	void OnMapOpened(const FString& Filename, bool bAsTemplate);

	void OnBlueprintCompiled();
	
	//////////////////////////////////////////////////////////////////////////
	/// Stats
public:
	bool bPreviewMode;
	bool bShowListPanel;
	bool bShowDetailPanel;
	bool bShowStatusPanel;

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
	FDelegateHandle OnMapOpenedHandle;
	FDelegateHandle OnBlueprintCompiledHandle;

public:
	void TogglePreviewMode();

	void SetIsPreviewMode(bool bIsPreviewMode);
};

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Widget/SEditorSlateWidgetBase.h"

/**
 * 
 */
class WHFRAMEWORKEDITOR_API STaskEditorWidget : public SEditorSlateWidgetBase
{
public:
	STaskEditorWidget();
	
	SLATE_BEGIN_ARGS(STaskEditorWidget) {}
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
	bool bPreviewMode = false;
	bool bShowListPanel = true;
	bool bShowDetailPanel = true;
	bool bShowStatusPanel = true;

	//////////////////////////////////////////////////////////////////////////
	/// Refs
public:
	class ATaskModule* TaskModule;

	//////////////////////////////////////////////////////////////////////////
	/// Widgets
protected:
	TSharedPtr<class STaskToolbarWidget> ToolbarWidget;

	TSharedPtr<class STaskListWidget> ListWidget;

	TSharedPtr<class STaskDetailWidget> DetailWidget;

	TSharedPtr<class STaskStatusWidget> StatusWidget;

private:
	FDelegateHandle OnBeginPIEHandle;
	FDelegateHandle OnEndPIEHandle;
	FDelegateHandle OnMapOpenedHandle;
	FDelegateHandle OnBlueprintCompiledHandle;

public:
	void TogglePreviewMode();

	void SetIsPreviewMode(bool bIsPreviewMode);
};

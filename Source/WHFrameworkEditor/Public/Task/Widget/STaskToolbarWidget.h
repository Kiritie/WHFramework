// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Widget/SEditorSlateWidgetBase.h"

/**
 * 
 */
class WHFRAMEWORKEDITOR_API STaskToolbarWidget : public SEditorSlateWidgetBase
{
public:
	STaskToolbarWidget();
	
	SLATE_BEGIN_ARGS(STaskToolbarWidget) {}

		SLATE_ARGUMENT(TSharedPtr<class STaskEditorWidget>, MainWidget)
		SLATE_ARGUMENT(TSharedPtr<class STaskListWidget>, ListWidget)

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
	TSharedPtr<class STaskEditorWidget> MainWidget;

	TSharedPtr<class STaskListWidget> ListWidget;

private:
	void OnPreviewModeToggled();

	void OnMultiModeToggled();

	void OnEditModeToggled();
};

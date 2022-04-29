// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Widgets/SCompoundWidget.h"

/**
 * 
 */
class WHFRAMEWORKEDITOR_API SProcedureToolbarWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SProcedureToolbarWidget) {}

		SLATE_ARGUMENT(TSharedPtr<class SProcedureEditorWidget>, MainWidget)
		SLATE_ARGUMENT(TSharedPtr<class SProcedureListWidget>, ListWidget)

	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);

	//////////////////////////////////////////////////////////////////////////
	/// Refs
public:
	TSharedPtr<class SProcedureEditorWidget> MainWidget;

	TSharedPtr<class SProcedureListWidget> ListWidget;

private:
	void OnPreviewModeToggled();

	void OnMultiModeToggled();

	void OnEditModeToggled();
};

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Widget/SEditorWidgetBase.h"

/**
 * 
 */
class WHFRAMEWORKEDITOR_API SProcedureToolbarWidget : public SEditorWidgetBase
{
public:
	SProcedureToolbarWidget();
	
	SLATE_BEGIN_ARGS(SProcedureToolbarWidget) {}

		SLATE_ARGUMENT(TSharedPtr<class SProcedureEditorWidget>, MainWidget)
		SLATE_ARGUMENT(TSharedPtr<class SProcedureListWidget>, ListWidget)

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
	TSharedPtr<class SProcedureEditorWidget> MainWidget;

	TSharedPtr<class SProcedureListWidget> ListWidget;

private:
	void OnPreviewModeToggled();

	void OnMultiModeToggled();

	void OnEditModeToggled();
};

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Widget/Base/SEditorWidgetBase.h"

/**
 * 
 */
class WHFRAMEWORKEDITOR_API SModuleToolbarWidget : public SEditorWidgetBase
{
public:
	SModuleToolbarWidget();
	
	SLATE_BEGIN_ARGS(SModuleToolbarWidget) {}

		SLATE_ARGUMENT(TSharedPtr<class SModuleEditorWidget>, MainWidget)
		SLATE_ARGUMENT(TSharedPtr<class SModuleListWidget>, ListWidget)

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
	TSharedPtr<class SModuleEditorWidget> MainWidget;

	TSharedPtr<class SModuleListWidget> ListWidget;

private:
	void OnPreviewModeToggled();

	void OnMultiModeToggled();

	void OnEditModeToggled();
};

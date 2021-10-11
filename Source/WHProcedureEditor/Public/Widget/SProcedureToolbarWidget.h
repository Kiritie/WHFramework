// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Widgets/SCompoundWidget.h"

/**
 * 
 */
class WHPROCEDUREEDITOR_API SProcedureToolbarWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SProcedureToolbarWidget)
	{}

	SLATE_ARGUMENT(TSharedPtr<class SProcedureMainWidget>, MainWidget)
	SLATE_ARGUMENT(TSharedPtr<class SProcedureListWidget>, ListWidget)
	
	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);

	//////////////////////////////////////////////////////////////////////////
	/// Refs
public:
	TSharedPtr<class SProcedureMainWidget> MainWidget;
	
	TSharedPtr<class SProcedureListWidget> ListWidget;

private:
	void OnEditModeToggled();

	void OnMultiModeToggled();
};

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Widgets/SCompoundWidget.h"

/**
 * 
 */
class WHPROCEDUREEDITOR_API SProcedureMainWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SProcedureMainWidget)
	{}
	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);

	//////////////////////////////////////////////////////////////////////////
	/// Stats
public:
	bool bShowListPanel = true;
	bool bShowDetailPanel = true;
	bool bShowStatusPanel = true;

	//////////////////////////////////////////////////////////////////////////
	/// Refs
public:
	class AMainModule* MainModule;

	class AProcedureModule* ProcedureModule;

	//////////////////////////////////////////////////////////////////////////
	/// Widgets
protected:
	TSharedPtr<class SProcedureToolbarWidget> ToolbarWidget;

	TSharedPtr<class SProcedureListWidget> ListWidget;

	TSharedPtr<class SProcedureDetailWidget> DetailWidget;

	TSharedPtr<class SProcedureStatusWidget> StatusWidget;
};

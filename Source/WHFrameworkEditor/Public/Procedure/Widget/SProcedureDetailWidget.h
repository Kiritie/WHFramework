// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Widgets/SCompoundWidget.h"
#include "Procedure/Base/ProcedureBase.h"

class SProcedureListWidget;
class AProcedureModule;
/**
 * 
 */
class WHFRAMEWORKEDITOR_API SProcedureDetailWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SProcedureDetailWidget)
	{}

	SLATE_ARGUMENT(AProcedureModule*, ProcedureModule)
	
	SLATE_ARGUMENT(TSharedPtr<SProcedureListWidget>, ListWidget)
	
	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);

	//////////////////////////////////////////////////////////////////////////
	/// Stats
public:
	TArray<TSharedPtr<FProcedureListItem>> SelectedProcedureListItems;

	//////////////////////////////////////////////////////////////////////////
	/// Refs
public:
	AProcedureModule* ProcedureModule;

	TSharedPtr<SProcedureListWidget> ListWidget;

	//////////////////////////////////////////////////////////////////////////
	/// Widgets
public:
	TSharedPtr<IDetailsView> DetailsView;

private:
	void UpdateDetailsView();

	void OnSelectProcedureListItem(TArray<TSharedPtr<FProcedureListItem>> ProcedureListItem);
};

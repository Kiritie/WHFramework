// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "SProcedureListWidget.h"
#include "Widgets/SCompoundWidget.h"

/**
 * 
 */
class WHFRAMEWORKEDITOR_API SProcedureListItemWidget : public STableRow<TSharedPtr<FProcedureListItem>>
{
public:
	SLATE_BEGIN_ARGS(SProcedureListItemWidget) {}

		SLATE_ARGUMENT(TSharedPtr<FProcedureListItem>, Item)

		SLATE_ARGUMENT(TSharedPtr<SProcedureListWidget>, ListWidget)

	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView);

	//////////////////////////////////////////////////////////////////////////
	/// Refs
public:
	TSharedPtr<FProcedureListItem> Item;
	
	TSharedPtr<SProcedureListWidget> ListWidget;
};

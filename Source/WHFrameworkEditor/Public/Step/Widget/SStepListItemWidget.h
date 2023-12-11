// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "SStepListWidget.h"
#include "Widgets/SCompoundWidget.h"

/**
 * 
 */
class WHFRAMEWORKEDITOR_API SStepListItemWidget : public STableRow<TSharedPtr<FStepListItem>>
{
public:
	SLATE_BEGIN_ARGS(SStepListItemWidget) {}

		SLATE_ARGUMENT(TSharedPtr<FStepListItem>, Item)

		SLATE_ARGUMENT(TSharedPtr<SStepListWidget>, ListWidget)

	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView);

	//////////////////////////////////////////////////////////////////////////
	/// Refs
public:
	TSharedPtr<FStepListItem> Item;
	
	TSharedPtr<SStepListWidget> ListWidget;
};

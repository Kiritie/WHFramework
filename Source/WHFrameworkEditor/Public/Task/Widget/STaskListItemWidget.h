// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "STaskListWidget.h"
#include "Widgets/SCompoundWidget.h"

/**
 * 
 */
class WHFRAMEWORKEDITOR_API STaskListItemWidget : public STableRow<TSharedPtr<FTaskListItem>>
{
public:
	SLATE_BEGIN_ARGS(STaskListItemWidget) {}

		SLATE_ARGUMENT(TSharedPtr<FTaskListItem>, Item)

	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView);

	//////////////////////////////////////////////////////////////////////////
	/// Refs
public:
	TSharedPtr<FTaskListItem> Item;
};

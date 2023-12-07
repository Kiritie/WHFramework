// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "SModuleListWidget.h"
#include "Widgets/SCompoundWidget.h"

/**
 * 
 */
class WHFRAMEWORKEDITOR_API SModuleListItemWidget : public STableRow<TSharedPtr<FModuleListItem>>
{
public:
	SLATE_BEGIN_ARGS(SModuleListItemWidget) {}

		SLATE_ARGUMENT(TSharedPtr<FModuleListItem>, Item)

	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView);

	//////////////////////////////////////////////////////////////////////////
	/// Refs
public:
	TSharedPtr<FModuleListItem> Item;
};

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "STaskListWidget.h"
#include "Widgets/SCompoundWidget.h"

/**
 * 
 */
class WHFRAMEWORKEDITOR_API STaskListItemWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(STaskListItemWidget) {}

	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments&, const TSharedPtr<const FTaskListItem>& InItem);

	//////////////////////////////////////////////////////////////////////////
	/// Refs
public:
	TSharedPtr<const FTaskListItem> Item;
};

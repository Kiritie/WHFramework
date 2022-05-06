// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "SStepListWidget.h"
#include "Widgets/SCompoundWidget.h"

/**
 * 
 */
class WHFRAMEWORKEDITOR_API SStepListItemWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SStepListItemWidget) {}

	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments&, const TSharedPtr<const FStepListItem>& InItem);

	//////////////////////////////////////////////////////////////////////////
	/// Refs
public:
	TSharedPtr<const FStepListItem> Item;
};

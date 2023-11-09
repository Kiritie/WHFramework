// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "SModuleListWidget.h"
#include "Widgets/SCompoundWidget.h"

/**
 * 
 */
class WHFRAMEWORKEDITOR_API SModuleListItemWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SModuleListItemWidget) {}

	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments&, const TSharedPtr<const FModuleListItem>& InItem);

	//////////////////////////////////////////////////////////////////////////
	/// Refs
public:
	TSharedPtr<const FModuleListItem> Item;
};

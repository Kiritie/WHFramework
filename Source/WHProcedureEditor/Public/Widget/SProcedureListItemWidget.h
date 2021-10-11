// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "SProcedureListWidget.h"
#include "Widgets/SCompoundWidget.h"

/**
 * 
 */
class WHPROCEDUREEDITOR_API SProcedureListItemWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SProcedureListItemWidget)
	{}
	
	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments&, const TSharedPtr<const FProcedureListItem>& InItem);

	//////////////////////////////////////////////////////////////////////////
	/// Refs
public:
	TSharedPtr<const FProcedureListItem> Item;
};

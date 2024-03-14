// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Widgets/SCompoundWidget.h"

class STabLabel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(STabLabel) {}

		SLATE_ARGUMENT(FText, Label)

		SLATE_ATTRIBUTE(bool, bActived)

	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);
};

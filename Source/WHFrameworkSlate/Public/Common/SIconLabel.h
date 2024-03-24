// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Widgets/SCompoundWidget.h"

class WHFRAMEWORKSLATE_API SIconLabel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SIconLabel) {}

		SLATE_ARGUMENT(const FSlateBrush*, Icon)

		SLATE_ARGUMENT(FText, Label)

	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);
};

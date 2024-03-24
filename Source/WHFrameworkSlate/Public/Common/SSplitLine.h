// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Widgets/SCompoundWidget.h"

class WHFRAMEWORKSLATE_API SSplitLine : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSplitLine)
		:_Color(FSlateColor(FLinearColor(0.05f, 0.05f, 0.05f)))
		{}

		SLATE_ARGUMENT(int32, Height)
		SLATE_ARGUMENT(FSlateColor, Color)

	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);
};

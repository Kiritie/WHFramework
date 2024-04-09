// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Widgets/SCompoundWidget.h"

class WHFRAMEWORKSLATE_API SEditorTabLabel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SEditorTabLabel)
		:_Color(FSlateColor::UseForeground())
	{}

		SLATE_ATTRIBUTE(const FSlateBrush*, Icon)

		SLATE_ARGUMENT(FText, Label)

		SLATE_ARGUMENT(FSlateColor, Color)

	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);
};

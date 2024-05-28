// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Widgets/SCompoundWidget.h"

class WHFRAMEWORKSLATE_API SEditorTabLabel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SEditorTabLabel)
		: _LabelFont(FCoreStyle::GetDefaultFontStyle("Regular", 11))
		, _LabelColor(FSlateColor::UseForeground())
	{}

		SLATE_ATTRIBUTE(const FSlateBrush*, Icon)

		SLATE_ATTRIBUTE(FText, Label)

		SLATE_ATTRIBUTE(FSlateFontInfo, LabelFont)

		SLATE_ATTRIBUTE(FSlateColor, LabelColor)

	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);
};

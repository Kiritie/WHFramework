// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SEditorTabLabel.h"

class WHFRAMEWORKSLATE_API SEditorTabLabel_UnderLine : public SEditorTabLabel
{
public:
	SLATE_BEGIN_ARGS(SEditorTabLabel_UnderLine)
		: _LabelFont(FCoreStyle::GetDefaultFontStyle("Regular", 11))
		, _LabelColor(FSlateColor::UseForeground())
		, _LabelPadding(FMargin(10.f, 0.f, 10.f, 15.f))
	{}
		SLATE_ATTRIBUTE(const FSlateBrush*, Icon)

		SLATE_ATTRIBUTE(FText, Label)

		SLATE_ATTRIBUTE(FSlateFontInfo, LabelFont)

		SLATE_ATTRIBUTE(FSlateColor, LabelColor)

		SLATE_ATTRIBUTE(FMargin, LabelPadding)

		SLATE_ATTRIBUTE(bool, bActived)

	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);
};

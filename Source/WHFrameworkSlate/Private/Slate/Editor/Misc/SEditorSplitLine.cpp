// Fill out your copyright notice in the Description page of Project Settings.

#include "Slate/Editor/Misc/SEditorSplitLine.h"

#include "SlateOptMacros.h"
#include "WHFrameworkSlateStyle.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SEditorSplitLine::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SBox)
		.WidthOverride(InArgs._Width)
		.HeightOverride(InArgs._Height)
		[
			SNew(SImage)
			.Image(FWHFrameworkSlateStyle::Get().GetBrush("Icons.Box"))
			.ColorAndOpacity(InArgs._Color)
		]
	];
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

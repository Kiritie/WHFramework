// Fill out your copyright notice in the Description page of Project Settings.

#include "Common/SSplitLine.h"

#include "SlateOptMacros.h"
#include "WHFrameworkSlateStyle.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SSplitLine::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SBox)
		.HeightOverride(InArgs._Height)
		[
			SNew(SImage)
			.Image(FWHFrameworkSlateStyle::Get().GetBrush("Icons.White"))
			.ColorAndOpacity(InArgs._Color)
		]
	];
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

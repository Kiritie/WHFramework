// Fill out your copyright notice in the Description page of Project Settings.

#include "Common/SIconLabel.h"

#include "SlateOptMacros.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SIconLabel::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SHorizontalBox)

		+SHorizontalBox::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.Padding(FMargin(0.f, 0.f, 15.f, 0.f))
		.AutoWidth()
		[
			SNew(SImage)
			.Image(InArgs._Icon)
			.Visibility_Lambda([InArgs]() { return InArgs._Icon ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed; })
		]
		
		+SHorizontalBox::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(STextBlock)
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 11))
			.Text(InArgs._Label)
		]
	];
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

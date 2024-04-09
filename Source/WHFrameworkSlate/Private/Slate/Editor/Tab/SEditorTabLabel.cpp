// Fill out your copyright notice in the Description page of Project Settings.

#include "Slate/Editor/Tab/SEditorTabLabel.h"

#include "SlateOptMacros.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SEditorTabLabel::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SHorizontalBox)

		+SHorizontalBox::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.Padding(FMargin(0.f, 0.f, 5.f, 0.f))
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(14.f)
			.HeightOverride(14.f)
			[
				SNew(SImage)
				.Image_Lambda([InArgs](){ return InArgs._Icon.Get(); })
				.Visibility_Lambda([InArgs]() { return InArgs._Icon.Get() ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed; })
			]
		]
		
		+SHorizontalBox::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(STextBlock)
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 11))
			.Text(InArgs._Label)
			.ColorAndOpacity(InArgs._Color)
		]
	];
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

// Fill out your copyright notice in the Description page of Project Settings.

#include "Slate/Editor/Tab/SEditorTabLabel_UnderLine.h"

#include "SlateOptMacros.h"
#include "Slate/Editor/Misc/SEditorSplitLine.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SEditorTabLabel_UnderLine::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SVerticalBox)

		+SVerticalBox::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		.Padding(FMargin(0.f, 0.f, 0.f, 15.f))
		.AutoHeight()
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
		]

		+SVerticalBox::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		.AutoHeight()
		[
			SNew(SEditorSplitLine)
			.Height(2.f)
			.Color(FSlateColor(FLinearColor(0.f, 0.4f, 0.95f)))
			.Visibility_Lambda([InArgs](){ return InArgs._bActived.Get() ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed; })
		]
	];
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

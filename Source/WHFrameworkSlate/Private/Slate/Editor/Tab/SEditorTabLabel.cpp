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
		.Padding(FMargin(0.f, 0.f, 4.f, 0.f))
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(14.f)
			.HeightOverride(14.f)
			[
				SNew(SImage)
				.Image(InArgs._Icon)
				.Visibility_Lambda([InArgs]() { return InArgs._Icon.Get() ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed; })
			]
		]
		
		+SHorizontalBox::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(STextBlock)
			.Text(InArgs._Label)
			.Font(InArgs._LabelFont)
			.ColorAndOpacity(InArgs._LabelColor)
		]
	];
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

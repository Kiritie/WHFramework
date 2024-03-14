// Fill out your copyright notice in the Description page of Project Settings.

#include "Common/STabLabel.h"

#include "SlateOptMacros.h"
#include "..\..\Public\Common\SSplitLine.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void STabLabel::Construct(const FArguments& InArgs)
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
			SNew(STextBlock)
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 12))
			.Text(InArgs._Label)
			.ColorAndOpacity_Lambda([InArgs](){ return InArgs._bActived.Get() ? FSlateColor(FLinearColor(1.f, 1.f, 1.f)) : FSlateColor::UseForeground(); })
		]

		+SVerticalBox::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		.AutoHeight()
		[
			SNew(SSplitLine)
			.Height(2.f)
			.Color(FSlateColor(FLinearColor(0.f, 0.4f, 0.95f)))
			.Visibility_Lambda([InArgs](){ return InArgs._bActived.Get() ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed; })
		]
	];
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

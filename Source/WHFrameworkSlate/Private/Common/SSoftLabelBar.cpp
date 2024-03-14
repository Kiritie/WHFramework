// Fill out your copyright notice in the Description page of Project Settings.

#include "Common/SSoftLabelBar.h"

#include "SlateOptMacros.h"
#include "WHFrameworkSlateStyle.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

SSoftLabelBar::SSoftLabelBar()
{
	SortLabelIndex = 0;
	SortLabelAscending = true;
}

void SSoftLabelBar::Construct(const FArguments& InArgs)
{
	SortLabelIndex = InArgs._SortLabelIndex.Get();
	SortLabelAscending = InArgs._SortLabelAscending.Get();
	
	OnSortLabelIndexAndAscendingChanged = InArgs._OnSortLabelIndexAndAscendingChanged;
	
	SAssignNew(HBox_Labels, SHorizontalBox);

	for(int32 i = 0; i < InArgs._SortLabels.Num(); i++)
	{
		HBox_Labels->AddSlot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		.FillWidth(InArgs._SortLabels[i].Fill)
		[
			SNew(SButton)
			.ButtonStyle(&FWHFrameworkSlateStyle::Get().GetWidgetStyle<FButtonStyle>("Main.Button.Tab"))
			.OnClicked_Lambda([this, InArgs, i]()
			{
				if(!InArgs._SortLabels[i].Label.IsEmpty())
				{
					if(SortLabelIndex != i)
					{
						SortLabelIndex = i;
						SortLabelAscending = true;
					}
					else
					{
						SortLabelAscending = !SortLabelAscending;
					}
					if(OnSortLabelIndexAndAscendingChanged.IsBound())
					{
						OnSortLabelIndexAndAscendingChanged.Execute(SortLabelIndex, SortLabelAscending);
					}
				}
				return FReply::Handled();
			})
			[
				SNew(SHorizontalBox)
	
				+SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Fill)
				.AutoWidth()
				[
					SNew(SBox)
					.WidthOverride(1.5f)
					[
						SNew(SImage)
						.Image(FWHFrameworkSlateStyle::Get().GetBrush("Icons.White"))
						.ColorAndOpacity(FSlateColor(FLinearColor(0.05f, 0.05f, 0.05f)))
					]
				]

				+SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				.Padding(FMargin(10.f, 0.f, 0.f, 0.f))
				.AutoWidth()
				[
					SNew(STextBlock)
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
					.Text(InArgs._SortLabels[i].Label)
				]

				+SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				.Padding(FMargin(10.f, 0.f, 0.f, 0.f))
				.AutoWidth()
				[
					SNew(SImage)
					.Visibility_Lambda([this, InArgs, i](){ return !InArgs._SortLabels[i].Label.IsEmpty() && SortLabelIndex == i ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed; })
					.Image_Lambda([this](){ return SortLabelAscending ? FWHFrameworkSlateStyle::Get().GetBrush("Icons.Arrow_Up") :  FWHFrameworkSlateStyle::Get().GetBrush("Icons.Arrow_Down"); })
				]
			]
		];
	}

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FWHFrameworkSlateStyle::Get().GetBrush("Icons.Border"))
		.BorderBackgroundColor(FLinearColor(0.05f, 0.05f, 0.05f))
		.Padding(InArgs._Padding)
		[
			HBox_Labels->AsShared()
		]
	];
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

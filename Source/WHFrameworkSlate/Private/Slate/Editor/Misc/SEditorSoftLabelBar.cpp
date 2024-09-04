// Fill out your copyright notice in the Description page of Project Settings.

#include "Slate/Editor/Misc/SEditorSoftLabelBar.h"

#include "SlateOptMacros.h"
#include "WHFrameworkSlateStyle.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

SEditorSoftLabelBar::SEditorSoftLabelBar()
{
	SortLabelIndex = 0;
	SortLabelAscending = true;
}

void SEditorSoftLabelBar::Construct(const FArguments& InArgs)
{
	SortLabelIndex = InArgs._SortLabelIndex.Get();
	SortLabelAscending = InArgs._SortLabelAscending.Get();
	SortLabels = InArgs._SortLabels;
	
	OnSortLabelIndexAndAscendingChanged = InArgs._OnSortLabelIndexAndAscendingChanged;
	
	SAssignNew(HBox_Labels, SHorizontalBox);

	for(int32 i = 0; i < SortLabels.Num(); i++)
	{
		HBox_Labels->AddSlot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		.FillWidth(SortLabels[i].Fill)
		[
			SNew(SButton)
			.ButtonStyle(&FWHFrameworkSlateStyle::Get().GetWidgetStyle<FButtonStyle>("Buttons.Tab.Default"))
			.ContentPadding(FMargin(0.f))
			.Visibility_Lambda([this, i](){ return SortLabels[i].bSortable ? EVisibility::Visible : EVisibility::HitTestInvisible; })
			.OnClicked_Lambda([this, i]()
			{
				if(!SortLabels[i].Label.Get().IsEmpty())
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
				.VAlign(VAlign_Center)
				.Padding(FMargin(0.f))
				.AutoWidth()
				[
					SNew(STextBlock)
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
					.Text(SortLabels[i].Label)
					// .ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.7f))
				]

				+SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				.Padding(FMargin(10.f, 0.f, 0.f, 0.f))
				.AutoWidth()
				[
					SNew(SImage)
					.Visibility_Lambda([this, i](){ return !SortLabels[i].Label.Get().IsEmpty() && SortLabelIndex == i ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed; })
					.Image_Lambda([this](){ return SortLabelAscending ? FWHFrameworkSlateStyle::Get().GetBrush("Icons.Arrow_Up") :  FWHFrameworkSlateStyle::Get().GetBrush("Icons.Arrow_Down"); })
				]
			]
		];
	}

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FWHFrameworkSlateStyle::Get().GetBrush("Icons.Border_Radius_8"))
		.BorderBackgroundColor(FLinearColor(1.f, 1.f, 1.f, 0.08f))
		.Padding(InArgs._Padding)
		[
			HBox_Labels->AsShared()
		]
	];
}

float SEditorSoftLabelBar::GetSortLabelFill(int32 InIndex) const
{
	if(SortLabels.IsValidIndex(InIndex))
	{
		return SortLabels[InIndex].Fill;
	}
	return 0.f;
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

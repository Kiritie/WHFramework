// Fill out your copyright notice in the Description page of Project Settings.


#include "Common/STabListItem.h"

#include "SlateOptMacros.h"
#include "WHFrameworkSlateStyle.h"
#include "Common/SIconLabel.h"
#include "Common/SSplitLine.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void STabListItem::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
{
	TabListItem = InArgs._TabListItem;
	
	STableRow::Construct
	(
		STableRow::FArguments()
		.Style(FWHFrameworkSlateStyle::Get(), "Main.List.Tab")
		.ShowWires(false)
		.Content()
		[
			SNew(SVerticalBox)

			+SVerticalBox::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.Padding(InArgs._Padding)
			.AutoHeight()
			[
				SNew(SIconLabel)
				.Icon(TabListItem->TabIcon)
				.Label(FText::FromString(TabListItem->TabName))
			]

			+SVerticalBox::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.AutoHeight()
			[
				SNew(SSplitLine)
				.Height(2.f)
				.Color(FSlateColor(FLinearColor(0.f, 0.4f, 0.95f)))
				.Visibility_Lambda([this](){ return !TabListItem->TabIcon && IsSelected() ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed; })
			]
		],
		InOwnerTableView
	);
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

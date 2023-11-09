// Fill out your copyright notice in the Description page of Project Settings.


#include "Main/Widget/SModuleListItemWidget.h"

#include "SlateOptMacros.h"
#include "Main/Base/ModuleBase.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SModuleListItemWidget::Construct(const FArguments&, const TSharedPtr<const FModuleListItem>& InItem)
{
	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
		.Padding(2.f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Left)
			[
				SNew(STextBlock)
				.Text_Lambda([InItem](){ return FText::FromString(FString::Printf(TEXT("%s"), *InItem->Module->GetModuleDisplayName().ToString())); })
			]
		]
	];
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

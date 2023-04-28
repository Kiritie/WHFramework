// Fill out your copyright notice in the Description page of Project Settings.


#include "Task/Widget/STaskListItemWidget.h"

#include "SlateOptMacros.h"
#include "Task/Base/TaskBase.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void STaskListItemWidget::Construct(const FArguments&, const TSharedPtr<const FTaskListItem>& InItem)
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
				.Text_Lambda([InItem](){ return FText::FromString(FString::Printf(TEXT("%d.%s"), InItem->Task->TaskIndex + 1, *InItem->Task->TaskDisplayName.ToString())); })
			]
		]
	];
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

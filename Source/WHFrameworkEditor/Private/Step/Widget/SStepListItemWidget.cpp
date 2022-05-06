// Fill out your copyright notice in the Description page of Project Settings.


#include "Step/Widget/SStepListItemWidget.h"

#include "SlateOptMacros.h"
#include "Step/Base/StepBase.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SStepListItemWidget::Construct(const FArguments&, const TSharedPtr<const FStepListItem>& InItem)
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
				.Text_Lambda([InItem](){ return FText::FromString(FString::Printf(TEXT("%d.%s"), InItem->Step->StepIndex + 1, *InItem->Step->StepDisplayName.ToString())); })
			]
		]
	];
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

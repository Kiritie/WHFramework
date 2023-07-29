// Fill out your copyright notice in the Description page of Project Settings.


#include "Step/Widget/SStepStatusWidget.h"
#include "SlateOptMacros.h"
#include "Step/Widget/SStepListWidget.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

SStepStatusWidget::SStepStatusWidget()
{
	WidgetName = FName("StepStatusWidget");
	WidgetType = EEditorWidgetType::Child;
}

void SStepStatusWidget::Construct(const FArguments& InArgs)
{
	SEditorWidgetBase::Construct(SEditorWidgetBase::FArguments());

	ListWidget = InArgs._ListWidget;

	if(!ListWidget) return;

	ChildSlot
	[
		SNew(SBorder)
		.Padding(1.f)
		.BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Fill)
			.FillHeight(1)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Fill)
				.HAlign(HAlign_Left)
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("  List:  ")))
					.ColorAndOpacity(FSlateColor(FLinearColor::Yellow))
				]

				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Fill)
				.HAlign(HAlign_Left)
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text_Lambda([this](){ return FText::FromString(FString::Printf(TEXT("Total Num: %d"), ListWidget->GetTotalStepNum())); })
				]

				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Fill)
				.HAlign(HAlign_Left)
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text_Lambda([this](){ return FText::FromString(FString::Printf(TEXT("  Selected Num: %d"), ListWidget->GetSelectedStepNum())); })
				]
			]
		]
	];
}

void SStepStatusWidget::OnCreate()
{
	SEditorWidgetBase::OnCreate();
}

void SStepStatusWidget::OnReset()
{
	SEditorWidgetBase::OnReset();
}

void SStepStatusWidget::OnRefresh()
{
	SEditorWidgetBase::OnRefresh();
}

void SStepStatusWidget::OnDestroy()
{
	SEditorWidgetBase::OnDestroy();
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

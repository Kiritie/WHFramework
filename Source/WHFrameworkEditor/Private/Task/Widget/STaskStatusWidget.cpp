// Fill out your copyright notice in the Description page of Project Settings.


#include "Task/Widget/STaskStatusWidget.h"
#include "SlateOptMacros.h"
#include "Task/Widget/STaskListWidget.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

STaskStatusWidget::STaskStatusWidget()
{
	WidgetName = FName("TaskStatusWidget");
	WidgetType = EEditorWidgetType::Child;
}

void STaskStatusWidget::Construct(const FArguments& InArgs)
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
					.Text(FText::FromString(TEXT("List:  ")))
					.ColorAndOpacity(FSlateColor(FLinearColor::Yellow))
				]

				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Fill)
				.HAlign(HAlign_Left)
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text_Lambda([this](){ return FText::FromString(FString::Printf(TEXT("Total Num: %d"), ListWidget->GetTotalTaskNum())); })
				]

				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Fill)
				.HAlign(HAlign_Left)
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text_Lambda([this](){ return FText::FromString(FString::Printf(TEXT("  Selected Num: %d"), ListWidget->GetSelectedTaskNum())); })
				]
			]
		]
	];
}

void STaskStatusWidget::OnCreate()
{
	SEditorWidgetBase::OnCreate();
}

void STaskStatusWidget::OnReset()
{
	SEditorWidgetBase::OnReset();
}

void STaskStatusWidget::OnRefresh()
{
	SEditorWidgetBase::OnRefresh();
}

void STaskStatusWidget::OnDestroy()
{
	SEditorWidgetBase::OnDestroy();
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

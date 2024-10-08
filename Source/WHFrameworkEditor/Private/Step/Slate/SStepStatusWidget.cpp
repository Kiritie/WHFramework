// Fill out your copyright notice in the Description page of Project Settings.

#include "Step/Slate/SStepStatusWidget.h"

#include "SlateOptMacros.h"
#include "Step/Slate/SStepListWidget.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

const FName SStepStatusWidget::WidgetName = FName("StepStatusWidget");

SStepStatusWidget::SStepStatusWidget()
{
	WidgetType = EEditorWidgetType::Child;
}

void SStepStatusWidget::Construct(const FArguments& InArgs)
{
	SEditorWidgetBase::Construct(SEditorWidgetBase::FArguments());
	
	StepEditor = InArgs._StepEditor;

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
					.Text_Lambda([this](){ return FText::FromString(FString::Printf(TEXT("Total Num: %d"), StepEditor.Pin()->ListWidget->GetTotalStepNum())); })
				]

				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Fill)
				.HAlign(HAlign_Left)
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text_Lambda([this](){ return FText::FromString(FString::Printf(TEXT("  Selected Num: %d"), StepEditor.Pin()->ListWidget->GetSelectedStepNum())); })
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

// Fill out your copyright notice in the Description page of Project Settings.


#include "Main/Widget/SModuleStatusWidget.h"
#include "SlateOptMacros.h"
#include "Main/Widget/SModuleListWidget.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

SModuleStatusWidget::SModuleStatusWidget()
{
	WidgetName = FName("ModuleStatusWidget");
	WidgetType = EEditorWidgetType::Child;
}

void SModuleStatusWidget::Construct(const FArguments& InArgs)
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
					.Text_Lambda([this](){ return FText::FromString(FString::Printf(TEXT("Total Num: %d"), ListWidget->GetTotalModuleNum())); })
				]

				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Fill)
				.HAlign(HAlign_Left)
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text_Lambda([this](){ return FText::FromString(FString::Printf(TEXT("  Selected Num: %d"), ListWidget->GetSelectedModuleNum())); })
				]
			]
		]
	];
}

void SModuleStatusWidget::OnCreate()
{
	SEditorWidgetBase::OnCreate();
}

void SModuleStatusWidget::OnReset()
{
	SEditorWidgetBase::OnReset();
}

void SModuleStatusWidget::OnRefresh()
{
	SEditorWidgetBase::OnRefresh();
}

void SModuleStatusWidget::OnDestroy()
{
	SEditorWidgetBase::OnDestroy();
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
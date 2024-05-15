// Fill out your copyright notice in the Description page of Project Settings.

#include "Main/Slate/SModuleStatusWidget.h"

#include "SlateOptMacros.h"
#include "Main/Slate/SModuleEditorWidget.h"
#include "Main/Slate/SModuleListWidget.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

FName SModuleStatusWidget::WidgetName = FName("ModuleStatusWidget");

SModuleStatusWidget::SModuleStatusWidget()
{
	WidgetType = EEditorWidgetType::Child;
}

void SModuleStatusWidget::Construct(const FArguments& InArgs)
{
	SEditorWidgetBase::Construct(SEditorWidgetBase::FArguments());
}

void SModuleStatusWidget::OnCreate()
{
	SEditorWidgetBase::OnCreate();

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
					.Text_Lambda([this](){ return FText::FromString(FString::Printf(TEXT("Total Num: %d"), GetParentWidgetN<SModuleEditorWidget>()->ListWidget->GetTotalModuleNum())); })
				]

				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Fill)
				.HAlign(HAlign_Left)
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text_Lambda([this](){ return FText::FromString(FString::Printf(TEXT("  Selected Num: %d"), GetParentWidgetN<SModuleEditorWidget>()->ListWidget->GetSelectedModuleNum())); })
				]
			]
		]
	];
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

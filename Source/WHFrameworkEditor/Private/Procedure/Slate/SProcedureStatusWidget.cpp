// Fill out your copyright notice in the Description page of Project Settings.

#include "Procedure/Slate/SProcedureStatusWidget.h"

#include "SlateOptMacros.h"
#include "Procedure/ProcedureEditor.h"
#include "Procedure/Slate/SProcedureListWidget.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

FName SProcedureStatusWidget::WidgetName = FName("ProcedureStatusWidget");

SProcedureStatusWidget::SProcedureStatusWidget()
{
	WidgetType = EEditorWidgetType::Child;
}

void SProcedureStatusWidget::Construct(const FArguments& InArgs)
{
	SEditorWidgetBase::Construct(SEditorWidgetBase::FArguments());
	
	ProcedureEditor = InArgs._ProcedureEditor;

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
					.Text_Lambda([this](){ return FText::FromString(FString::Printf(TEXT("Total Num: %d"), ProcedureEditor.Pin()->ListWidget->GetTotalProcedureNum())); })
				]

				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Fill)
				.HAlign(HAlign_Left)
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text_Lambda([this](){ return FText::FromString(FString::Printf(TEXT("  Selected Num: %d"), ProcedureEditor.Pin()->ListWidget->GetSelectedProcedureNum())); })
				]
			]
		]
	];
}

void SProcedureStatusWidget::OnCreate()
{
	SEditorWidgetBase::OnCreate();
}

void SProcedureStatusWidget::OnReset()
{
	SEditorWidgetBase::OnReset();
}

void SProcedureStatusWidget::OnRefresh()
{
	SEditorWidgetBase::OnRefresh();
}

void SProcedureStatusWidget::OnDestroy()
{
	SEditorWidgetBase::OnDestroy();
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

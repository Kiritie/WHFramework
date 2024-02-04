// Fill out your copyright notice in the Description page of Project Settings.

#include "Debug/Widget/SDebugPanelWidget.h"
#include "SlateOptMacros.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

SDebugPanelWidget::SDebugPanelWidget()
{
	WidgetName = FName("DebugPanel");
}

void SDebugPanelWidget::Construct(const FArguments& InArgs)
{
	SSlateWidgetBase::Construct(SSlateWidgetBase::FArguments());
}

void SDebugPanelWidget::OnCreate(UObject* InOwner, const TArray<FParameter>& InParams)
{
	SSlateWidgetBase::OnCreate(InOwner, InParams);

	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Fill)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Fill)
			.Padding(5.f)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("Debug Message...")))
					.ColorAndOpacity(FSlateColor(FLinearColor::White))
				]
			]
		]
	];
}

void SDebugPanelWidget::OnOpen(const TArray<FParameter>& InParams, bool bInstant)
{
	SSlateWidgetBase::OnOpen(InParams, bInstant);
}

void SDebugPanelWidget::OnClose(bool bInstant)
{
	SSlateWidgetBase::OnClose(bInstant);
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

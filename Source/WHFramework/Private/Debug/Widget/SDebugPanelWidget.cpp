// Fill out your copyright notice in the Description page of Project Settings.

#include "Debug/Widget/SDebugPanelWidget.h"
#include "SlateOptMacros.h"
#include "Main/MainModuleStatics.h"
#include "Main/Base/ModuleBase.h"
#include "Widgets/Layout/SWrapBox.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

const FName SDebugPanelWidget::WidgetName = FName("DebugPanel");
const FName SDebugPanelWidget::ParentName = NAME_None;

SDebugPanelWidget::SDebugPanelWidget()
{
	WidgetZOrder = 9999;
}

void SDebugPanelWidget::Construct(const FArguments& InArgs)
{
	SSlateWidgetBase::Construct(SSlateWidgetBase::FArguments());
}

void SDebugPanelWidget::OnCreate(UObject* InOwner, const TArray<FParameter>& InParams)
{
	SSlateWidgetBase::OnCreate(InOwner, InParams);

	const TSharedPtr<SWrapBox> MessageBox = SNew(SWrapBox)
		.Orientation(Orient_Vertical)
		.UseAllottedWidth(true);

	for(auto Iter : UMainModuleStatics::GetAllModule())
	{
		MessageBox->AddSlot()
		.Padding(5.0f)
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Top)
		.Padding(FMargin(20.f))
		[
			SNew(SVerticalBox)
			
			+ SVerticalBox::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Left)
			.AutoHeight()
			.Padding(FMargin(0.f, 0.f, 0.f, 5.f))
			[
				SNew(STextBlock)
				.Text_Lambda([Iter]() { return FText::FromString(FString::Printf(TEXT("[%s]"), *Iter->GetModuleDisplayName().ToString())); })
				.ColorAndOpacity_Lambda([Iter](){ return !Iter->IsModuleRequired() ? FSlateColor(FLinearColor(1.f, 1.f, 1.f)) : FSlateColor(FLinearColor(1.f, 0.5f, 0.1f)); })
				.ShadowOffset(FVector2D(0.5f, 0.5f))
				.ShadowColorAndOpacity(FLinearColor(0.1f, 0.1f, 0.1f))
			]
			+ SVerticalBox::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Left)
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text_Lambda([Iter]() { return FText::FromString(Iter->GetModuleDebugMessage()); })
				.ColorAndOpacity(FLinearColor(0.85f, 0.85f, 0.85f))
				.ShadowOffset(FVector2D(0.5f, 0.5f))
				.ShadowColorAndOpacity(FLinearColor(0.1f, 0.1f, 0.1f))
			]
		];
	}

	ChildSlot
	[
		SNew(SVerticalBox)
		.Visibility(EVisibility::HitTestInvisible)

		+ SVerticalBox::Slot()
		.VAlign(VAlign_Fill)
		.HAlign(HAlign_Fill)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Fill)
			.Padding(FMargin(50.f))
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
				.BorderBackgroundColor(FLinearColor(0.3f, 0.3f, 0.3f, 0.3f))
				[
					MessageBox->AsShared()
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

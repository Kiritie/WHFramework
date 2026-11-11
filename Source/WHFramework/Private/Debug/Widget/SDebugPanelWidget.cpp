// Fill out your copyright notice in the Description page of Project Settings.

#include "Debug/Widget/SDebugPanelWidget.h"
#include "SlateOptMacros.h"
#include "Main/MainModuleStatics.h"
#include "Main/Base/ModuleBase.h"
#include "ObjectPool/ObjectPoolModuleTypes.h"
#include "Engine/World.h"
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

void SDebugPanelWidget::OnCreate(const FParameter& InParams)
{
	SSlateWidgetBase::OnCreate(InParams);
	const FWidgetSpawnParameter* Spawn = InParams.GetPtr<FWidgetSpawnParameter>();
	PanelWorld = Spawn ? Cast<UWorld>(Spawn->OwnerObject.Get()) : nullptr;

	const TSharedPtr<SWrapBox> MessageBox = SNew(SWrapBox)
		.Orientation(Orient_Vertical)
		.UseAllottedSize(true);

	for(UModuleBase* Iter : UMainModuleStatics::GetAllModule(PanelWorld.Get()))
	{
		const TWeakObjectPtr<UModuleBase> WeakModule(Iter);
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
				.Text_Lambda([WeakModule]() { return WeakModule.IsValid() ? FText::FromString(FString::Printf(TEXT("[%s]"), *WeakModule->GetModuleDisplayName().ToString())) : FText::GetEmpty(); })
				.ColorAndOpacity_Lambda([WeakModule](){ return WeakModule.IsValid() && WeakModule->IsModuleRequired() ? FSlateColor(FLinearColor(1.f, 0.5f, 0.1f)) : FSlateColor(FLinearColor(1.f, 1.f, 1.f)); })
				.ShadowOffset(FVector2D(0.5f, 0.5f))
				.ShadowColorAndOpacity(FLinearColor(0.1f, 0.1f, 0.1f))
			]
			+ SVerticalBox::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Left)
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text_Lambda([WeakModule]() { return WeakModule.IsValid() ? FText::FromString(WeakModule->GetModuleDebugMessage()) : FText::GetEmpty(); })
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

UGameViewportClient* SDebugPanelWidget::ResolveViewport() const
{
	return PanelWorld.IsValid() ? PanelWorld->GetGameViewport() : nullptr;
}

void SDebugPanelWidget::OnOpen(const FParameter& InParams, bool bInstant)
{
	SSlateWidgetBase::OnOpen(InParams, bInstant);
}

void SDebugPanelWidget::OnClose(bool bInstant)
{
	SSlateWidgetBase::OnClose(bInstant);
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

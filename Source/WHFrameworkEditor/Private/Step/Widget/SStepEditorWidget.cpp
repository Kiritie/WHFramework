// Fill out your copyright notice in the Description page of Project Settings.


#include "Step/Widget/SStepEditorWidget.h"

#include "Main/MainModule.h"
#include "SlateOptMacros.h"
#include "WHFrameworkEditor.h"
#include "WHFrameworkEditorStyle.h"
#include "Kismet/GameplayStatics.h"
#include "Main/MainModuleBPLibrary.h"
#include "Step/StepModule.h"
#include "Step/StepModuleBPLibrary.h"
#include "Step/Widget/SStepDetailWidget.h"
#include "Step/Widget/SStepListWidget.h"
#include "Step/Widget/SStepStatusWidget.h"
#include "Step/Widget/SStepToolbarWidget.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SStepEditorWidget::Construct(const FArguments& InArgs)
{
	SEditorSlateWidgetBase::Construct(SEditorSlateWidgetBase::FArguments());
	
	const FWHFrameworkEditorModule& WHFrameworkModule = FModuleManager::GetModuleChecked<FWHFrameworkEditorModule>(FName("WHFrameworkEditor"));
	const bool bPlaying = WHFrameworkModule.bPlaying;

	StepModule = AMainModule::GetModuleByClass<AStepModule>(!bPlaying);

	if(BeginPIEDelegateHandle.IsValid())
	{
		FEditorDelegates::PostPIEStarted.Remove(BeginPIEDelegateHandle);
	}
	BeginPIEDelegateHandle = FEditorDelegates::PostPIEStarted.AddRaw(this, &SStepEditorWidget::OnBeginPIE);

	if(EndPIEDelegateHandle.IsValid())
	{
		FEditorDelegates::EndPIE.Remove(EndPIEDelegateHandle);
	}
	EndPIEDelegateHandle = FEditorDelegates::EndPIE.AddRaw(this, &SStepEditorWidget::OnEndPIE);

	if(StepModule)
	{
		GConfig->GetBool(TEXT("/Script/WHFrameworkEditor.StepEditorSettings"), TEXT("bShowListPanel"), bShowListPanel, GStepEditorIni);
		GConfig->GetBool(TEXT("/Script/WHFrameworkEditor.StepEditorSettings"), TEXT("bShowDetailPanel"), bShowDetailPanel, GStepEditorIni);
		GConfig->GetBool(TEXT("/Script/WHFrameworkEditor.StepEditorSettings"), TEXT("bShowStatusPanel"), bShowStatusPanel, GStepEditorIni);

		SAssignNew(ListWidget, SStepListWidget)
			.StepModule(StepModule)
			.Visibility_Lambda([this](){ return bShowListPanel ? EVisibility::Visible : EVisibility::Collapsed; });
		AddChild(ListWidget);

		SAssignNew(DetailWidget, SStepDetailWidget)
			.StepModule(StepModule)
			.ListWidget(ListWidget)
			.Visibility_Lambda([this](){ return bShowDetailPanel ? EVisibility::Visible : EVisibility::Collapsed; });
		AddChild(DetailWidget);

		SAssignNew(StatusWidget, SStepStatusWidget)
			.ListWidget(ListWidget)
			.Visibility_Lambda([this](){ return bShowStatusPanel ? EVisibility::Visible : EVisibility::Collapsed; });
		AddChild(StatusWidget);

		SAssignNew(ToolbarWidget, SStepToolbarWidget)
			.MainWidget(SharedThis(this))
			.ListWidget(ListWidget);
		AddChild(ToolbarWidget);

		ChildSlot
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Fill)
			.AutoHeight()
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Fill)
				.HAlign(HAlign_Fill)
				.FillWidth(1)
				.Padding(1)
				[
					ToolbarWidget.ToSharedRef()
				]
			]

			+ SVerticalBox::Slot()
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Fill)
			.FillHeight(1)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Fill)
				.HAlign(HAlign_Fill)
				.Padding(1)
				.AutoWidth()
				[
					ListWidget.ToSharedRef()
				]

				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Fill)
				.HAlign(HAlign_Fill)
				.FillWidth(1)

				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Fill)
				.HAlign(HAlign_Fill)
				.Padding(1)
				.AutoWidth()
				[
					DetailWidget.ToSharedRef()
				]
			]

			+ SVerticalBox::Slot()
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Fill)
			.AutoHeight()
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Fill)
				.HAlign(HAlign_Fill)
				.Padding(1)
				.FillWidth(1)
				[
					StatusWidget.ToSharedRef()
				]
			]
		];

		SetIsPreviewMode(bPlaying);
	}
	else
	{
		ChildSlot
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Fill)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("No Step module found, please close this window!")))
					.ColorAndOpacity(FSlateColor(FLinearColor::Red))
				]
			]
		];
	}
}

void SStepEditorWidget::OnCreate()
{
	SEditorSlateWidgetBase::OnCreate();

	if(RefreshDelegateHandle.IsValid())
	{
		GEditor->OnBlueprintCompiled().Remove(RefreshDelegateHandle);
	}
	RefreshDelegateHandle = GEditor->OnBlueprintCompiled().AddRaw(this, &SStepEditorWidget::Refresh);
}

void SStepEditorWidget::OnReset()
{
	SEditorSlateWidgetBase::OnReset();
}

void SStepEditorWidget::OnRefresh()
{
	StepModule = AMainModule::GetModuleByClass<AStepModule>(!bPreviewMode);
	if(StepModule)
	{
		if(DetailWidget)
		{
			DetailWidget->StepModule = StepModule;
		}
		if(ListWidget)
		{
			ListWidget->StepModule = StepModule;
		}

		SEditorSlateWidgetBase::OnRefresh();
	}
}

void SStepEditorWidget::OnDestroy()
{
	SEditorSlateWidgetBase::OnDestroy();

	if(RefreshDelegateHandle.IsValid())
	{
		GEditor->OnBlueprintCompiled().Remove(RefreshDelegateHandle);
	}

	if(BeginPIEDelegateHandle.IsValid())
	{
		FEditorDelegates::PostPIEStarted.Remove(BeginPIEDelegateHandle);
	}

	if(EndPIEDelegateHandle.IsValid())
	{
		FEditorDelegates::EndPIE.Remove(EndPIEDelegateHandle);
	}
}

void SStepEditorWidget::TogglePreviewMode()
{
	SetIsPreviewMode(!bPreviewMode);
}

void SStepEditorWidget::OnBeginPIE(bool bIsSimulating)
{
	if(!bIsSimulating)
	{
		SetIsPreviewMode(true);
	}
}

void SStepEditorWidget::OnEndPIE(bool bIsSimulating)
{
	if(!bIsSimulating)
	{
		SetIsPreviewMode(false);
	}
}

void SStepEditorWidget::SetIsPreviewMode(bool bIsPreviewMode)
{
	if(bPreviewMode != bIsPreviewMode)
	{
		if(AStepModule* _StepModule = AMainModule::GetModuleByClass<AStepModule>(!bIsPreviewMode))
		{
			bPreviewMode = bIsPreviewMode;
			StepModule = _StepModule;
			if(ListWidget)
			{
				DetailWidget->StepModule = StepModule;
			}
			if(ListWidget)
			{
				ListWidget->StepModule = StepModule;
				ListWidget->Refresh();
			}
			SetRenderOpacity(bPreviewMode ? 0.8f : 1.f);
		}
	}
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

// Fill out your copyright notice in the Description page of Project Settings.


#include "Procedure/Widget/SProcedureEditorWidget.h"

#include "Main/MainModule.h"
#include "SlateOptMacros.h"
#include "WHFrameworkEditor.h"
#include "WHFrameworkEditorStyle.h"
#include "Kismet/GameplayStatics.h"
#include "Main/MainModuleBPLibrary.h"
#include "Procedure/ProcedureModule.h"
#include "Procedure/ProcedureModuleBPLibrary.h"
#include "Procedure/Widget/SProcedureDetailWidget.h"
#include "Procedure/Widget/SProcedureListWidget.h"
#include "Procedure/Widget/SProcedureStatusWidget.h"
#include "Procedure/Widget/SProcedureToolbarWidget.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SProcedureEditorWidget::Construct(const FArguments& InArgs)
{
	const FWHFrameworkEditorModule& WHFrameworkModule = FModuleManager::GetModuleChecked<FWHFrameworkEditorModule>(FName("WHFrameworkEditor"));
	const bool bPlaying = WHFrameworkModule.bPlaying;

	ProcedureModule = AMainModule::GetModuleByClass<AProcedureModule>(!bPlaying);

	if(BeginPIEDelegateHandle.IsValid())
	{
		FEditorDelegates::PostPIEStarted.Remove(BeginPIEDelegateHandle);
	}
	BeginPIEDelegateHandle = FEditorDelegates::PostPIEStarted.AddRaw(this, &SProcedureEditorWidget::OnBeginPIE);

	if(EndPIEDelegateHandle.IsValid())
	{
		FEditorDelegates::EndPIE.Remove(EndPIEDelegateHandle);
	}
	EndPIEDelegateHandle = FEditorDelegates::EndPIE.AddRaw(this, &SProcedureEditorWidget::OnEndPIE);

	if(ProcedureModule)
	{
		GConfig->GetBool(TEXT("/Script/WHFrameworkEditor.ProcedureEditorSettings"), TEXT("bShowListPanel"), bShowListPanel, GProcedureEditorIni);
		GConfig->GetBool(TEXT("/Script/WHFrameworkEditor.ProcedureEditorSettings"), TEXT("bShowDetailPanel"), bShowDetailPanel, GProcedureEditorIni);
		GConfig->GetBool(TEXT("/Script/WHFrameworkEditor.ProcedureEditorSettings"), TEXT("bShowStatusPanel"), bShowStatusPanel, GProcedureEditorIni);

		SAssignNew(ListWidget, SProcedureListWidget)
			.ProcedureModule(ProcedureModule)
			.Visibility_Lambda([this](){ return bShowListPanel ? EVisibility::Visible : EVisibility::Collapsed; });

		SAssignNew(DetailWidget, SProcedureDetailWidget)
			.ProcedureModule(ProcedureModule)
			.ListWidget(ListWidget)
			.Visibility_Lambda([this](){ return bShowDetailPanel ? EVisibility::Visible : EVisibility::Collapsed; });

		SAssignNew(StatusWidget, SProcedureStatusWidget)
			.ListWidget(ListWidget)
			.Visibility_Lambda([this](){ return bShowStatusPanel ? EVisibility::Visible : EVisibility::Collapsed; });

		SAssignNew(ToolbarWidget, SProcedureToolbarWidget)
			.MainWidget(SharedThis(this))
			.ListWidget(ListWidget);

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
					.Text(FText::FromString(TEXT("No procedure module found, please close this window!")))
					.ColorAndOpacity(FSlateColor(FLinearColor::Red))
				]
			]
		];
	}
}

void SProcedureEditorWidget::TogglePreviewMode()
{
	SetIsPreviewMode(!bPreviewMode);
}

void SProcedureEditorWidget::OnBeginPIE(bool bIsSimulating)
{
	if(!bIsSimulating)
	{
		SetIsPreviewMode(true);
	}
}

void SProcedureEditorWidget::OnEndPIE(bool bIsSimulating)
{
	if(!bIsSimulating)
	{
		SetIsPreviewMode(false);
	}
}

void SProcedureEditorWidget::SetIsPreviewMode(bool bIsPreviewMode)
{
	if(bPreviewMode != bIsPreviewMode)
	{
		if(AProcedureModule* _ProcedureModule = AMainModule::GetModuleByClass<AProcedureModule>(!bIsPreviewMode))
		{
			bPreviewMode = bIsPreviewMode;
			ProcedureModule = _ProcedureModule;
			if(ListWidget)
			{
				DetailWidget->ProcedureModule = ProcedureModule;
			}
			if(ListWidget)
			{
				ListWidget->ProcedureModule = ProcedureModule;
				ListWidget->Refresh();
			}
			SetRenderOpacity(bPreviewMode ? 0.8f : 1.f);
		}
	}
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

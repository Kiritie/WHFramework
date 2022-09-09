// Fill out your copyright notice in the Description page of Project Settings.


#include "Procedure/Widget/SProcedureEditorWidget.h"

#include "Main/MainModule.h"
#include "SlateOptMacros.h"
#include "WHFrameworkEditor.h"
#include "WHFrameworkEditorStyle.h"
#include "Global/GlobalBPLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Main/MainModuleBPLibrary.h"
#include "Procedure/ProcedureModule.h"
#include "Procedure/ProcedureModuleBPLibrary.h"
#include "Procedure/Widget/SProcedureDetailWidget.h"
#include "Procedure/Widget/SProcedureListWidget.h"
#include "Procedure/Widget/SProcedureStatusWidget.h"
#include "Procedure/Widget/SProcedureToolbarWidget.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

SProcedureEditorWidget::SProcedureEditorWidget()
{
	WidgetName = FName("ProcedureEditorWidget");
	WidgetType = EEditorWidgetType::Main;

	bPreviewMode = false;
	bShowListPanel = true;
	bShowDetailPanel = true;
	bShowStatusPanel = true;

	ProcedureModule = nullptr;
}

void SProcedureEditorWidget::Construct(const FArguments& InArgs)
{
	SEditorSlateWidgetBase::Construct(SEditorSlateWidgetBase::FArguments());

	ProcedureModule = AProcedureModule::Get(!UGlobalBPLibrary::IsPlaying());

	if(ProcedureModule)
	{
		GConfig->GetBool(TEXT("/Script/WHFrameworkEditor.ProcedureEditorSettings"), TEXT("bShowListPanel"), bShowListPanel, GProcedureEditorIni);
		GConfig->GetBool(TEXT("/Script/WHFrameworkEditor.ProcedureEditorSettings"), TEXT("bShowDetailPanel"), bShowDetailPanel, GProcedureEditorIni);
		GConfig->GetBool(TEXT("/Script/WHFrameworkEditor.ProcedureEditorSettings"), TEXT("bShowStatusPanel"), bShowStatusPanel, GProcedureEditorIni);

		SAssignNew(ListWidget, SProcedureListWidget)
			.ProcedureModule(ProcedureModule)
			.Visibility_Lambda([this](){ return bShowListPanel ? EVisibility::Visible : EVisibility::Collapsed; });
		AddChild(ListWidget);

		SAssignNew(DetailWidget, SProcedureDetailWidget)
			.ProcedureModule(ProcedureModule)
			.ListWidget(ListWidget)
			.Visibility_Lambda([this](){ return bShowDetailPanel ? EVisibility::Visible : EVisibility::Collapsed; });
		AddChild(DetailWidget);

		SAssignNew(StatusWidget, SProcedureStatusWidget)
			.ListWidget(ListWidget)
			.Visibility_Lambda([this](){ return bShowStatusPanel ? EVisibility::Visible : EVisibility::Collapsed; });
		AddChild(StatusWidget);

		SAssignNew(ToolbarWidget, SProcedureToolbarWidget)
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

		SetIsPreviewMode(UGlobalBPLibrary::IsPlaying());
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

void SProcedureEditorWidget::OnCreate()
{
	SEditorSlateWidgetBase::OnCreate();

	OnBeginPIEHandle = FEditorDelegates::PostPIEStarted.AddRaw(this, &SProcedureEditorWidget::OnBeginPIE);

	OnEndPIEHandle = FEditorDelegates::EndPIE.AddRaw(this, &SProcedureEditorWidget::OnEndPIE);

	OnMapOpenedHandle = FEditorDelegates::OnMapOpened.AddRaw(this, &SProcedureEditorWidget::OnMapOpened);

	OnBlueprintCompiledHandle = GEditor->OnBlueprintCompiled().AddRaw(this, &SProcedureEditorWidget::OnBlueprintCompiled);
}

void SProcedureEditorWidget::OnReset()
{
	SEditorSlateWidgetBase::OnReset();
}

void SProcedureEditorWidget::OnRefresh()
{
	ProcedureModule = AProcedureModule::Get(!bPreviewMode);
	if(ProcedureModule)
	{
		if(ListWidget)
		{
			ListWidget->ProcedureModule = ProcedureModule;
		}
		if(DetailWidget)
		{
			DetailWidget->ProcedureModule = ProcedureModule;
		}
		SEditorSlateWidgetBase::OnRefresh();
	}
	else
	{
		Destroy();
	}
}

void SProcedureEditorWidget::OnDestroy()
{
	SEditorSlateWidgetBase::OnDestroy();

	if(OnBeginPIEHandle.IsValid())
	{
		FEditorDelegates::PostPIEStarted.Remove(OnBeginPIEHandle);
	}

	if(OnEndPIEHandle.IsValid())
	{
		FEditorDelegates::EndPIE.Remove(OnEndPIEHandle);
	}

	if(OnMapOpenedHandle.IsValid())
	{
		FEditorDelegates::OnMapOpened.Remove(OnMapOpenedHandle);
	}

	if(OnBlueprintCompiledHandle.IsValid())
	{
		GEditor->OnBlueprintCompiled().Remove(OnBlueprintCompiledHandle);
	}
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

void SProcedureEditorWidget::OnMapOpened(const FString& Filename, bool bAsTemplate)
{
	Refresh();
}

void SProcedureEditorWidget::OnBlueprintCompiled()
{
	Refresh();
}

void SProcedureEditorWidget::TogglePreviewMode()
{
	SetIsPreviewMode(!bPreviewMode);
}

void SProcedureEditorWidget::SetIsPreviewMode(bool bIsPreviewMode)
{
	if(bPreviewMode != bIsPreviewMode)
	{
		bPreviewMode = bIsPreviewMode;
		Refresh();
		SetRenderOpacity(bPreviewMode ? 0.8f : 1.f);
	}
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

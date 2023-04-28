// Fill out your copyright notice in the Description page of Project Settings.


#include "Task/Widget/STaskEditorWidget.h"

#include "Main/MainModule.h"
#include "SlateOptMacros.h"
#include "WHFrameworkEditor.h"
#include "WHFrameworkEditorStyle.h"
#include "Global/GlobalBPLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Main/MainModuleBPLibrary.h"
#include "Task/TaskModule.h"
#include "Task/TaskModuleBPLibrary.h"
#include "Task/Widget/STaskDetailWidget.h"
#include "Task/Widget/STaskListWidget.h"
#include "Task/Widget/STaskStatusWidget.h"
#include "Task/Widget/STaskToolbarWidget.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

STaskEditorWidget::STaskEditorWidget()
{
	WidgetName = FName("TaskEditorWidget");
	WidgetType = EEditorWidgetType::Main;

	bPreviewMode = false;
	bShowListPanel = true;
	bShowDetailPanel = true;
	bShowStatusPanel = true;

	TaskModule = nullptr;
}

void STaskEditorWidget::Construct(const FArguments& InArgs)
{
	SEditorSlateWidgetBase::Construct(SEditorSlateWidgetBase::FArguments());

	TaskModule = ATaskModule::Get(!UGlobalBPLibrary::IsPlaying());

	if(TaskModule)
	{
		GConfig->GetBool(TEXT("/Script/WHFrameworkEditor.TaskEditorSettings"), TEXT("bShowListPanel"), bShowListPanel, GTaskEditorIni);
		GConfig->GetBool(TEXT("/Script/WHFrameworkEditor.TaskEditorSettings"), TEXT("bShowDetailPanel"), bShowDetailPanel, GTaskEditorIni);
		GConfig->GetBool(TEXT("/Script/WHFrameworkEditor.TaskEditorSettings"), TEXT("bShowStatusPanel"), bShowStatusPanel, GTaskEditorIni);

		SAssignNew(ListWidget, STaskListWidget)
			.TaskModule(TaskModule)
			.Visibility_Lambda([this](){ return bShowListPanel ? EVisibility::Visible : EVisibility::Collapsed; });
		AddChild(ListWidget);

		SAssignNew(DetailWidget, STaskDetailWidget)
			.TaskModule(TaskModule)
			.ListWidget(ListWidget)
			.Visibility_Lambda([this](){ return bShowDetailPanel ? EVisibility::Visible : EVisibility::Collapsed; });
		AddChild(DetailWidget);

		SAssignNew(StatusWidget, STaskStatusWidget)
			.ListWidget(ListWidget)
			.Visibility_Lambda([this](){ return bShowStatusPanel ? EVisibility::Visible : EVisibility::Collapsed; });
		AddChild(StatusWidget);

		SAssignNew(ToolbarWidget, STaskToolbarWidget)
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
					.Text(FText::FromString(TEXT("No Task module found, please close this window!")))
					.ColorAndOpacity(FSlateColor(FLinearColor::Red))
				]
			]
		];
	}
}

void STaskEditorWidget::OnCreate()
{
	SEditorSlateWidgetBase::OnCreate();

	OnBeginPIEHandle = FEditorDelegates::PostPIEStarted.AddRaw(this, &STaskEditorWidget::OnBeginPIE);

	OnEndPIEHandle = FEditorDelegates::EndPIE.AddRaw(this, &STaskEditorWidget::OnEndPIE);

	OnMapOpenedHandle = FEditorDelegates::OnMapOpened.AddRaw(this, &STaskEditorWidget::OnMapOpened);

	OnBlueprintCompiledHandle = GEditor->OnBlueprintCompiled().AddRaw(this, &STaskEditorWidget::OnBlueprintCompiled);
}


void STaskEditorWidget::OnReset()
{
	SEditorSlateWidgetBase::OnReset();
}

void STaskEditorWidget::OnRefresh()
{
	TaskModule = ATaskModule::Get(!bPreviewMode);
	if(TaskModule)
	{
		if(ListWidget)
		{
			ListWidget->TaskModule = TaskModule;
		}
		if(DetailWidget)
		{
			DetailWidget->TaskModule = TaskModule;
		}
		SEditorSlateWidgetBase::OnRefresh();
	}
	else
	{
		Destroy();
	}
}

void STaskEditorWidget::OnDestroy()
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

void STaskEditorWidget::OnBeginPIE(bool bIsSimulating)
{
	if(!bIsSimulating)
	{
		SetIsPreviewMode(true);
	}
}

void STaskEditorWidget::OnEndPIE(bool bIsSimulating)
{
	if(!bIsSimulating)
	{
		SetIsPreviewMode(false);
	}
}

void STaskEditorWidget::OnMapOpened(const FString& Filename, bool bAsTemplate)
{
	Refresh();
}

void STaskEditorWidget::OnBlueprintCompiled()
{
	Refresh();
}

void STaskEditorWidget::TogglePreviewMode()
{
	SetIsPreviewMode(!bPreviewMode);
}

void STaskEditorWidget::SetIsPreviewMode(bool bIsPreviewMode)
{
	if(bPreviewMode != bIsPreviewMode)
	{
		bPreviewMode = bIsPreviewMode;
		Refresh();
		SetRenderOpacity(bPreviewMode ? 0.8f : 1.f);
	}
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

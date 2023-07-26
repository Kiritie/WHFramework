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
#include "Task/Widget/STaskDetailsWidget.h"
#include "Task/Widget/STaskListWidget.h"
#include "Task/Widget/STaskStatusWidget.h"
#include "Task/Widget/STaskToolbarWidget.h"

#define LOCTEXT_NAMESPACE "TaskEditorWidget"

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
	SEditorWidgetBase::Construct(SEditorWidgetBase::FArguments());

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

		SAssignNew(DetailsWidget, STaskDetailsWidget)
			.TaskModule(TaskModule)
			.ListWidget(ListWidget)
			.Visibility_Lambda([this](){ return bShowDetailPanel ? EVisibility::Visible : EVisibility::Collapsed; });
		AddChild(DetailsWidget);

		SAssignNew(StatusWidget, STaskStatusWidget)
			.ListWidget(ListWidget)
			.Visibility_Lambda([this](){ return bShowStatusPanel ? EVisibility::Visible : EVisibility::Collapsed; });
		AddChild(StatusWidget);

		SAssignNew(ToolbarWidget, STaskToolbarWidget)
			.MainWidget(SharedThis(this))
			.ListWidget(ListWidget);
		AddChild(ToolbarWidget);

		const TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout("TaskEditor_Layout")
		->AddArea
		(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Vertical)
			->Split
			(
				FTabManager::NewStack()
				->SetHideTabWell(true)
				->SetSizeCoefficient(0.2f)
				->AddTab("Toolbar", ETabState::OpenedTab)
			)
			->Split
			(
				// Main application area
				FTabManager::NewSplitter()
				->SetOrientation(Orient_Horizontal)
				->SetSizeCoefficient(1.f)
				->Split
				(
					FTabManager::NewStack()
					->SetHideTabWell(false)
					->SetSizeCoefficient(0.35f)
					->AddTab("List", ETabState::OpenedTab)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetHideTabWell(false)
					->SetSizeCoefficient(0.3f)
					->AddTab("", ETabState::OpenedTab)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetHideTabWell(false)
					->SetSizeCoefficient(0.35f)
					->AddTab("Details", ETabState::OpenedTab)
				)
			)
			->Split
			(
				FTabManager::NewStack()
				->SetHideTabWell(true)
				->SetSizeCoefficient(0.1f)
				->AddTab("Status", ETabState::OpenedTab)
			)
		);

		auto RegisterTrackedTabSpawner = [this](const FName& TabId, const FOnSpawnTab& OnSpawnTab) -> FTabSpawnerEntry&
		{
			return TabManager->RegisterTabSpawner(TabId, FOnSpawnTab::CreateLambda([this, OnSpawnTab](const FSpawnTabArgs& Args) -> TSharedRef<SDockTab>
			{
				TSharedRef<SDockTab> SpawnedTab = OnSpawnTab.Execute(Args);
				OnTabSpawned(Args.GetTabId().TabType, SpawnedTab);
				return SpawnedTab;
			}));
		};

        const TSharedRef<SDockTab> NomadTab = SNew(SDockTab).TabRole(ETabRole::MajorTab);
		TabManager = FGlobalTabmanager::Get()->NewTabManager(NomadTab);
		TabManager->SetOnPersistLayout(FTabManager::FOnPersistLayout::CreateRaw(this, &STaskEditorWidget::HandleTabManagerPersistLayout));

		RegisterTrackedTabSpawner("Toolbar", FOnSpawnTab::CreateSP(this, &STaskEditorWidget::SpawnToolbarWidgetTab))
			.SetDisplayName(LOCTEXT("ToolbarTab", "Toolbar"))
			.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Toolbar"));

		RegisterTrackedTabSpawner("List", FOnSpawnTab::CreateSP(this, &STaskEditorWidget::SpawnListWidgetTab))
			.SetDisplayName(LOCTEXT("ListTab", "List"))
			.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Outliner"));

		RegisterTrackedTabSpawner("Details", FOnSpawnTab::CreateSP(this, &STaskEditorWidget::SpawnDetailsWidgetTab))
			.SetDisplayName(LOCTEXT("DetailsTab", "Details"))
			.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));

		RegisterTrackedTabSpawner("Status", FOnSpawnTab::CreateSP(this, &STaskEditorWidget::SpawnStatusWidgetTab))
			.SetDisplayName(LOCTEXT("StatusTab", "Status"))
			.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.StatsViewer"));

		if (GIsEditor)
		{
			//Layout = FLayoutSaveRestore::LoadFromConfig(GEditorLayoutIni, Layout);
		}

		FMenuBarBuilder MenuBarBuilder = FMenuBarBuilder(TSharedPtr<FUICommandList>());
		MenuBarBuilder.AddPullDownMenu(
			LOCTEXT("WindowMenuLabel", "Window"),
			FText::GetEmpty(),
			FNewMenuDelegate::CreateSP(this, &STaskEditorWidget::HandlePullDownWindowMenu),
			"Window"
		);
		
		const TSharedRef<SWidget> MenuWidget = MenuBarBuilder.MakeWidget();
		TabManager->SetMenuMultiBox(MenuBarBuilder.GetMultiBox(), MenuWidget);

		ChildSlot
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
			.BorderBackgroundColor(FLinearColor::Gray) // Darken the outer border
			[
				SNew(SVerticalBox)
		
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(0.0f, 4.0f, 0.0f, 0.0f))
				[
					MenuWidget
				]
				
				+ SVerticalBox::Slot()
				.Padding(FMargin(0.0f, 4.0f, 0.0f, 0.0f))
				[
					TabManager->RestoreFrom(Layout, nullptr).ToSharedRef()
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
	SEditorWidgetBase::OnCreate();

	OnBeginPIEHandle = FEditorDelegates::PostPIEStarted.AddRaw(this, &STaskEditorWidget::OnBeginPIE);

	OnEndPIEHandle = FEditorDelegates::EndPIE.AddRaw(this, &STaskEditorWidget::OnEndPIE);

	OnMapOpenedHandle = FEditorDelegates::OnMapOpened.AddRaw(this, &STaskEditorWidget::OnMapOpened);

	OnBlueprintCompiledHandle = GEditor->OnBlueprintCompiled().AddRaw(this, &STaskEditorWidget::OnBlueprintCompiled);
}


void STaskEditorWidget::OnReset()
{
	SEditorWidgetBase::OnReset();
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
		if(DetailsWidget)
		{
			DetailsWidget->TaskModule = TaskModule;
		}
		SEditorWidgetBase::OnRefresh();
	}
	else
	{
		Destroy();
	}
}

void STaskEditorWidget::OnDestroy()
{
	SEditorWidgetBase::OnDestroy();

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

void STaskEditorWidget::OnTabSpawned(const FName& TabIdentifier, const TSharedRef<SDockTab>& SpawnedTab)
{
	TWeakPtr<SDockTab>* const ExistingTab = SpawnedTabs.Find(TabIdentifier);
	if (!ExistingTab)
	{
		SpawnedTabs.Add(TabIdentifier, SpawnedTab);
	}
	else
	{
		check(!ExistingTab->IsValid());
		*ExistingTab = SpawnedTab;
	}
}

void STaskEditorWidget::HandleTabManagerPersistLayout(const TSharedRef<FTabManager::FLayout>& LayoutToSave)
{
	if (FUnrealEdMisc::Get().IsSavingLayoutOnClosedAllowed())
	{
		FLayoutSaveRestore::SaveToConfig(GEditorLayoutIni, LayoutToSave);
	}
}

TSharedRef<SDockTab> STaskEditorWidget::SpawnToolbarWidgetTab(const FSpawnTabArgs& Args)
{
	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Label(LOCTEXT("ToolbarTab", "Toolbar"))
		.ShouldAutosize(true)
		[
			ToolbarWidget.ToSharedRef()
		];
	return SpawnedTab;
}

TSharedRef<SDockTab> STaskEditorWidget::SpawnListWidgetTab(const FSpawnTabArgs& Args)
{
	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Label(LOCTEXT("ListTab", "List"))
		.ShouldAutosize(false)
		[
			ListWidget.ToSharedRef()
		];
	return SpawnedTab;
}

TSharedRef<SDockTab> STaskEditorWidget::SpawnDetailsWidgetTab(const FSpawnTabArgs& Args)
{
	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Label(LOCTEXT("DetailsTab", "Details"))
		.ShouldAutosize(false)
		[
			DetailsWidget.ToSharedRef()
		];
	return SpawnedTab;
}

TSharedRef<SDockTab> STaskEditorWidget::SpawnStatusWidgetTab(const FSpawnTabArgs& Args)
{
	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Label(LOCTEXT("StatusTab", "Status"))
		.ShouldAutosize(true)
		[
			StatusWidget.ToSharedRef()
		];
	return SpawnedTab;
}

void STaskEditorWidget::HandlePullDownWindowMenu(FMenuBuilder& MenuBuilder)
{
	if (!TabManager.IsValid())
	{
		return;
	}

	TabManager->PopulateLocalTabSpawnerMenu(MenuBuilder);
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

#undef LOCTEXT_NAMESPACE

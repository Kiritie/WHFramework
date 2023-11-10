// Fill out your copyright notice in the Description page of Project Settings.


#include "Step/Widget/SStepEditorWidget.h"

#include "LevelEditorActions.h"
#include "SlateOptMacros.h"
#include "Common/CommonStatics.h"
#include "Step/StepEditor.h"
#include "Step/StepModule.h"
#include "Step/Widget/SStepDetailsWidget.h"
#include "Step/Widget/SStepListWidget.h"
#include "Step/Widget/SStepStatusWidget.h"
#include "Step/Widget/SStepToolbarWidget.h"

#define LOCTEXT_NAMESPACE "StepEditorWidget"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

SStepEditorWidget::SStepEditorWidget()
{
	WidgetName = FName("StepEditorWidget");
	WidgetType = EEditorWidgetType::Main;

	bPreviewMode = false;
	bShowListPanel = true;
	bShowDetailPanel = true;
	bShowStatusPanel = true;

	StepModule = nullptr;
}

void SStepEditorWidget::Construct(const FArguments& InArgs)
{
	SEditorWidgetBase::Construct(SEditorWidgetBase::FArguments());

	StepModule = &UStepModule::Get(!UCommonStatics::IsPlaying());

	if(StepModule)
	{
		GConfig->GetBool(TEXT("/Script/WHFrameworkEditor.StepEditorSettings"), TEXT("bShowListPanel"), bShowListPanel, GStepEditorIni);
		GConfig->GetBool(TEXT("/Script/WHFrameworkEditor.StepEditorSettings"), TEXT("bShowDetailPanel"), bShowDetailPanel, GStepEditorIni);
		GConfig->GetBool(TEXT("/Script/WHFrameworkEditor.StepEditorSettings"), TEXT("bShowStatusPanel"), bShowStatusPanel, GStepEditorIni);

		SAssignNew(ListWidget, SStepListWidget)
			.StepModule(StepModule)
			.Visibility_Lambda([this](){ return bShowListPanel ? EVisibility::Visible : EVisibility::Collapsed; });
		AddChild(ListWidget);

		SAssignNew(DetailsWidget, SStepDetailsWidget)
			.StepModule(StepModule)
			.ListWidget(ListWidget)
			.Visibility_Lambda([this](){ return bShowDetailPanel ? EVisibility::Visible : EVisibility::Collapsed; });
		AddChild(DetailsWidget);

		SAssignNew(StatusWidget, SStepStatusWidget)
			.ListWidget(ListWidget)
			.Visibility_Lambda([this](){ return bShowStatusPanel ? EVisibility::Visible : EVisibility::Collapsed; });
		AddChild(StatusWidget);

		SAssignNew(ToolbarWidget, SStepToolbarWidget)
			.MainWidget(SharedThis(this))
			.ListWidget(ListWidget);
		AddChild(ToolbarWidget);

		const TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout("StepEditor_Layout")
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
					->SetSizeCoefficient(0.5f)
					->AddTab("List", ETabState::OpenedTab)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetHideTabWell(false)
					->SetSizeCoefficient(0.5f)
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
		TabManager->SetOnPersistLayout(FTabManager::FOnPersistLayout::CreateRaw(this, &SStepEditorWidget::HandleTabManagerPersistLayout));

		RegisterTrackedTabSpawner("Toolbar", FOnSpawnTab::CreateSP(this, &SStepEditorWidget::SpawnToolbarWidgetTab))
			.SetDisplayName(LOCTEXT("ToolbarTab", "Toolbar"))
			.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Toolbar"));

		RegisterTrackedTabSpawner("List", FOnSpawnTab::CreateSP(this, &SStepEditorWidget::SpawnListWidgetTab))
			.SetDisplayName(LOCTEXT("ListTab", "List"))
			.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Outliner"));

		RegisterTrackedTabSpawner("Details", FOnSpawnTab::CreateSP(this, &SStepEditorWidget::SpawnDetailsWidgetTab))
			.SetDisplayName(LOCTEXT("DetailsTab", "Details"))
			.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));

		RegisterTrackedTabSpawner("Status", FOnSpawnTab::CreateSP(this, &SStepEditorWidget::SpawnStatusWidgetTab))
			.SetDisplayName(LOCTEXT("StatusTab", "Status"))
			.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.StatsViewer"));

		if (GIsEditor)
		{
			//Layout = FLayoutSaveRestore::LoadFromConfig(GEditorLayoutIni, Layout);
		}

		FMenuBarBuilder MenuBarBuilder = FMenuBarBuilder(TSharedPtr<FUICommandList>());
		MenuBarBuilder.AddPullDownMenu(
			LOCTEXT("FileMenuLabel", "File"),
			FText::GetEmpty(),
			FNewMenuDelegate::CreateSP(this, &SStepEditorWidget::HandlePullDownFileMenu),
			"File"
		);
		
		MenuBarBuilder.AddPullDownMenu(
			LOCTEXT("WindowMenuLabel", "Window"),
			FText::GetEmpty(),
			FNewMenuDelegate::CreateSP(this, &SStepEditorWidget::HandlePullDownWindowMenu),
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
		
		SetIsPreviewMode(UCommonStatics::IsPlaying());
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

void SStepEditorWidget::OnBindCommands()
{
	SEditorWidgetBase::OnBindCommands();

	WidgetCommands->MapAction(
		FStepEditorCommands::Get().Save,
		FExecuteAction::CreateSP(this, &SStepEditorWidget::Save),
		FCanExecuteAction::CreateSP(this, &SStepEditorWidget::CanSave)
	);
}

void SStepEditorWidget::OnCreate()
{
	SEditorWidgetBase::OnCreate();

	OnBeginPIEHandle = FEditorDelegates::PostPIEStarted.AddRaw(this, &SStepEditorWidget::OnBeginPIE);

	OnEndPIEHandle = FEditorDelegates::EndPIE.AddRaw(this, &SStepEditorWidget::OnEndPIE);

	OnMapOpenedHandle = FEditorDelegates::OnMapOpened.AddRaw(this, &SStepEditorWidget::OnMapOpened);

	OnBlueprintCompiledHandle = GEditor->OnBlueprintCompiled().AddRaw(this, &SStepEditorWidget::OnBlueprintCompiled);
}

void SStepEditorWidget::OnSave()
{
	SEditorWidgetBase::OnSave();

	FLevelEditorActionCallbacks::Save();
}

void SStepEditorWidget::OnReset()
{
	SEditorWidgetBase::OnReset();
}

void SStepEditorWidget::OnRefresh()
{
	StepModule = &UStepModule::Get(!bPreviewMode);
	if(StepModule)
	{
		if(ListWidget)
		{
			ListWidget->StepModule = StepModule;
		}
		if(DetailsWidget)
		{
			DetailsWidget->StepModule = StepModule;
		}
		SEditorWidgetBase::OnRefresh();
	}
	else
	{
		Destroy();
	}
}

void SStepEditorWidget::OnDestroy()
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

void SStepEditorWidget::OnMapOpened(const FString& Filename, bool bAsTemplate)
{
	Refresh();
}

void SStepEditorWidget::OnBlueprintCompiled()
{
	Refresh();
}

void SStepEditorWidget::OnTabSpawned(const FName& TabIdentifier, const TSharedRef<SDockTab>& SpawnedTab)
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

void SStepEditorWidget::HandleTabManagerPersistLayout(const TSharedRef<FTabManager::FLayout>& LayoutToSave)
{
	if (FUnrealEdMisc::Get().IsSavingLayoutOnClosedAllowed())
	{
		FLayoutSaveRestore::SaveToConfig(GEditorLayoutIni, LayoutToSave);
	}
}

TSharedRef<SDockTab> SStepEditorWidget::SpawnToolbarWidgetTab(const FSpawnTabArgs& Args)
{
	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Label(LOCTEXT("ToolbarTab", "Toolbar"))
		.ShouldAutosize(true)
		[
			ToolbarWidget.ToSharedRef()
		];
	return SpawnedTab;
}

TSharedRef<SDockTab> SStepEditorWidget::SpawnListWidgetTab(const FSpawnTabArgs& Args)
{
	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Label(LOCTEXT("ListTab", "List"))
		.ShouldAutosize(false)
		[
			ListWidget.ToSharedRef()
		];
	return SpawnedTab;
}

TSharedRef<SDockTab> SStepEditorWidget::SpawnDetailsWidgetTab(const FSpawnTabArgs& Args)
{
	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Label(LOCTEXT("DetailsTab", "Details"))
		.ShouldAutosize(false)
		[
			DetailsWidget.ToSharedRef()
		];
	return SpawnedTab;
}

TSharedRef<SDockTab> SStepEditorWidget::SpawnStatusWidgetTab(const FSpawnTabArgs& Args)
{
	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Label(LOCTEXT("StatusTab", "Status"))
		.ShouldAutosize(true)
		[
			StatusWidget.ToSharedRef()
		];
	return SpawnedTab;
}

void SStepEditorWidget::HandlePullDownFileMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.AddMenuEntry(FStepEditorCommands::Get().Save);
}

void SStepEditorWidget::HandlePullDownWindowMenu(FMenuBuilder& MenuBuilder)
{
	if (!TabManager.IsValid())
	{
		return;
	}

	TabManager->PopulateLocalTabSpawnerMenu(MenuBuilder);
}

void SStepEditorWidget::TogglePreviewMode()
{
	SetIsPreviewMode(!bPreviewMode);
}

void SStepEditorWidget::SetIsPreviewMode(bool bIsPreviewMode)
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

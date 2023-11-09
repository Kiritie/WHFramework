// Fill out your copyright notice in the Description page of Project Settings.


#include "Main/Widget/SModuleEditorWidget.h"

#include "SlateOptMacros.h"
#include "Common/CommonStatics.h"
#include "Main/MainModule.h"
#include "Main/Widget/SModuleDetailsWidget.h"
#include "Main/Widget/SModuleListWidget.h"
#include "Main/Widget/SModuleStatusWidget.h"
#include "Main/Widget/SModuleToolbarWidget.h"

#define LOCTEXT_NAMESPACE "ModuleEditorWidget"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

SModuleEditorWidget::SModuleEditorWidget()
{
	WidgetName = FName("ModuleEditorWidget");
	WidgetType = EEditorWidgetType::Main;

	bPreviewMode = false;
	bShowListPanel = true;
	bShowDetailsPanel = true;
	bShowStatusPanel = true;
}

void SModuleEditorWidget::Construct(const FArguments& InArgs)
{
	SEditorWidgetBase::Construct(SEditorWidgetBase::FArguments());

	MainModule = AMainModule::Get(!UCommonStatics::IsPlaying());

	if(MainModule)
	{
		GConfig->GetBool(TEXT("/Script/WHFrameworkEditor.ModuleEditorSettings"), TEXT("bShowListPanel"), bShowListPanel, GModuleEditorIni);
		GConfig->GetBool(TEXT("/Script/WHFrameworkEditor.ModuleEditorSettings"), TEXT("bShowDetailsPanel"), bShowDetailsPanel, GModuleEditorIni);
		GConfig->GetBool(TEXT("/Script/WHFrameworkEditor.ModuleEditorSettings"), TEXT("bShowStatusPanel"), bShowStatusPanel, GModuleEditorIni);

		SAssignNew(ListWidget, SModuleListWidget)
			.MainModule(MainModule)
			.Visibility_Lambda([this](){ return bShowListPanel ? EVisibility::Visible : EVisibility::Collapsed; });
		AddChild(ListWidget);

		SAssignNew(DetailsWidget, SModuleDetailsWidget)
			.MainModule(MainModule)
			.ListWidget(ListWidget)
			.Visibility_Lambda([this](){ return bShowDetailsPanel ? EVisibility::Visible : EVisibility::Collapsed; });
		AddChild(DetailsWidget);

		SAssignNew(StatusWidget, SModuleStatusWidget)
			.ListWidget(ListWidget)
			.Visibility_Lambda([this](){ return bShowStatusPanel ? EVisibility::Visible : EVisibility::Collapsed; });
		AddChild(StatusWidget);

		SAssignNew(ToolbarWidget, SModuleToolbarWidget)
			.MainWidget(SharedThis(this))
			.ListWidget(ListWidget);
		AddChild(ToolbarWidget);

		const TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout("ModuleEditor_Layout")
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
		TabManager->SetOnPersistLayout(FTabManager::FOnPersistLayout::CreateRaw(this, &SModuleEditorWidget::HandleTabManagerPersistLayout));

		RegisterTrackedTabSpawner("Toolbar", FOnSpawnTab::CreateSP(this, &SModuleEditorWidget::SpawnToolbarWidgetTab))
			.SetDisplayName(LOCTEXT("ToolbarTab", "Toolbar"))
			.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Toolbar"));

		RegisterTrackedTabSpawner("List", FOnSpawnTab::CreateSP(this, &SModuleEditorWidget::SpawnListWidgetTab))
			.SetDisplayName(LOCTEXT("ListTab", "List"))
			.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Outliner"));

		RegisterTrackedTabSpawner("Details", FOnSpawnTab::CreateSP(this, &SModuleEditorWidget::SpawnDetailsWidgetTab))
			.SetDisplayName(LOCTEXT("DetailsTab", "Details"))
			.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));

		RegisterTrackedTabSpawner("Status", FOnSpawnTab::CreateSP(this, &SModuleEditorWidget::SpawnStatusWidgetTab))
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
			FNewMenuDelegate::CreateSP(this, &SModuleEditorWidget::HandlePullDownWindowMenu),
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
					.Text(FText::FromString(TEXT("No main module found, please close this window!")))
					.ColorAndOpacity(FSlateColor(FLinearColor::Red))
				]
			]
		];
	}
}

void SModuleEditorWidget::OnCreate()
{
	SEditorWidgetBase::OnCreate();

	OnBeginPIEHandle = FEditorDelegates::PostPIEStarted.AddRaw(this, &SModuleEditorWidget::OnBeginPIE);

	OnEndPIEHandle = FEditorDelegates::EndPIE.AddRaw(this, &SModuleEditorWidget::OnEndPIE);

	OnMapOpenedHandle = FEditorDelegates::OnMapOpened.AddRaw(this, &SModuleEditorWidget::OnMapOpened);

	OnBlueprintCompiledHandle = GEditor->OnBlueprintCompiled().AddRaw(this, &SModuleEditorWidget::OnBlueprintCompiled);
}

void SModuleEditorWidget::OnReset()
{
	SEditorWidgetBase::OnReset();
}

void SModuleEditorWidget::OnRefresh()
{
	MainModule = AMainModule::Get(!bPreviewMode);
	if(MainModule)
	{
		if(ListWidget)
		{
			ListWidget->MainModule = MainModule;
		}
		if(DetailsWidget)
		{
			DetailsWidget->MainModule = MainModule;
		}
		SEditorWidgetBase::OnRefresh();
	}
	else
	{
		Destroy();
	}
}

void SModuleEditorWidget::OnDestroy()
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

void SModuleEditorWidget::OnBeginPIE(bool bIsSimulating)
{
	if(!bIsSimulating)
	{
		SetIsPreviewMode(true);
	}
}

void SModuleEditorWidget::OnEndPIE(bool bIsSimulating)
{
	if(!bIsSimulating)
	{
		SetIsPreviewMode(false);
	}
}

void SModuleEditorWidget::OnMapOpened(const FString& Filename, bool bAsTemplate)
{
	Refresh();
}

void SModuleEditorWidget::OnBlueprintCompiled()
{
	Refresh();
}

void SModuleEditorWidget::OnTabSpawned(const FName& TabIdentifier, const TSharedRef<SDockTab>& SpawnedTab)
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

void SModuleEditorWidget::HandleTabManagerPersistLayout(const TSharedRef<FTabManager::FLayout>& LayoutToSave)
{
	if (FUnrealEdMisc::Get().IsSavingLayoutOnClosedAllowed())
	{
		FLayoutSaveRestore::SaveToConfig(GEditorLayoutIni, LayoutToSave);
	}
}

TSharedRef<SDockTab> SModuleEditorWidget::SpawnToolbarWidgetTab(const FSpawnTabArgs& Args)
{
	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Label(LOCTEXT("ToolbarTab", "Toolbar"))
		.ShouldAutosize(true)
		[
			ToolbarWidget.ToSharedRef()
		];
	return SpawnedTab;
}

TSharedRef<SDockTab> SModuleEditorWidget::SpawnListWidgetTab(const FSpawnTabArgs& Args)
{
	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Label(LOCTEXT("ListTab", "List"))
		.ShouldAutosize(false)
		[
			ListWidget.ToSharedRef()
		];
	return SpawnedTab;
}

TSharedRef<SDockTab> SModuleEditorWidget::SpawnDetailsWidgetTab(const FSpawnTabArgs& Args)
{
	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Label(LOCTEXT("DetailsTab", "Details"))
		.ShouldAutosize(false)
		[
			DetailsWidget.ToSharedRef()
		];
	return SpawnedTab;
}

TSharedRef<SDockTab> SModuleEditorWidget::SpawnStatusWidgetTab(const FSpawnTabArgs& Args)
{
	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Label(LOCTEXT("StatusTab", "Status"))
		.ShouldAutosize(true)
		[
			StatusWidget.ToSharedRef()
		];
	return SpawnedTab;
}

void SModuleEditorWidget::HandlePullDownWindowMenu(FMenuBuilder& MenuBuilder)
{
	if (!TabManager.IsValid())
	{
		return;
	}

	TabManager->PopulateLocalTabSpawnerMenu(MenuBuilder);
}

void SModuleEditorWidget::TogglePreviewMode()
{
	SetIsPreviewMode(!bPreviewMode);
}

void SModuleEditorWidget::SetIsPreviewMode(bool bIsPreviewMode)
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

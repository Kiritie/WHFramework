// Fill out your copyright notice in the Description page of Project Settings.


#include "Procedure/Widget/SProcedureEditorWidget.h"

#include "SlateOptMacros.h"
#include "Common/CommonStatics.h"
#include "Procedure/ProcedureModule.h"
#include "Procedure/Widget/SProcedureDetailsWidget.h"
#include "Procedure/Widget/SProcedureListWidget.h"
#include "Procedure/Widget/SProcedureStatusWidget.h"
#include "Procedure/Widget/SProcedureToolbarWidget.h"

#define LOCTEXT_NAMESPACE "ProcedureEditorWidget"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

SProcedureEditorWidget::SProcedureEditorWidget()
{
	WidgetName = FName("ProcedureEditorWidget");
	WidgetType = EEditorWidgetType::Main;

	bPreviewMode = false;
	bShowListPanel = true;
	bShowDetailsPanel = true;
	bShowStatusPanel = true;

	ProcedureModule = nullptr;
}

void SProcedureEditorWidget::Construct(const FArguments& InArgs)
{
	SEditorWidgetBase::Construct(SEditorWidgetBase::FArguments());

	ProcedureModule = UProcedureModule::Get(!UCommonStatics::IsPlaying());

	if(ProcedureModule)
	{
		GConfig->GetBool(TEXT("/Script/WHFrameworkEditor.ProcedureEditorSettings"), TEXT("bShowListPanel"), bShowListPanel, GProcedureEditorIni);
		GConfig->GetBool(TEXT("/Script/WHFrameworkEditor.ProcedureEditorSettings"), TEXT("bShowDetailsPanel"), bShowDetailsPanel, GProcedureEditorIni);
		GConfig->GetBool(TEXT("/Script/WHFrameworkEditor.ProcedureEditorSettings"), TEXT("bShowStatusPanel"), bShowStatusPanel, GProcedureEditorIni);

		SAssignNew(ListWidget, SProcedureListWidget)
			.ProcedureModule(ProcedureModule)
			.Visibility_Lambda([this](){ return bShowListPanel ? EVisibility::Visible : EVisibility::Collapsed; });
		AddChild(ListWidget);

		SAssignNew(DetailsWidget, SProcedureDetailsWidget)
			.ProcedureModule(ProcedureModule)
			.ListWidget(ListWidget)
			.Visibility_Lambda([this](){ return bShowDetailsPanel ? EVisibility::Visible : EVisibility::Collapsed; });
		AddChild(DetailsWidget);

		SAssignNew(StatusWidget, SProcedureStatusWidget)
			.ListWidget(ListWidget)
			.Visibility_Lambda([this](){ return bShowStatusPanel ? EVisibility::Visible : EVisibility::Collapsed; });
		AddChild(StatusWidget);

		SAssignNew(ToolbarWidget, SProcedureToolbarWidget)
			.MainWidget(SharedThis(this))
			.ListWidget(ListWidget);
		AddChild(ToolbarWidget);

		const TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout("ProcedureEditor_Layout")
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
		TabManager->SetOnPersistLayout(FTabManager::FOnPersistLayout::CreateRaw(this, &SProcedureEditorWidget::HandleTabManagerPersistLayout));

		RegisterTrackedTabSpawner("Toolbar", FOnSpawnTab::CreateSP(this, &SProcedureEditorWidget::SpawnToolbarWidgetTab))
			.SetDisplayName(LOCTEXT("ToolbarTab", "Toolbar"))
			.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Toolbar"));

		RegisterTrackedTabSpawner("List", FOnSpawnTab::CreateSP(this, &SProcedureEditorWidget::SpawnListWidgetTab))
			.SetDisplayName(LOCTEXT("ListTab", "List"))
			.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Outliner"));

		RegisterTrackedTabSpawner("Details", FOnSpawnTab::CreateSP(this, &SProcedureEditorWidget::SpawnDetailsWidgetTab))
			.SetDisplayName(LOCTEXT("DetailsTab", "Details"))
			.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));

		RegisterTrackedTabSpawner("Status", FOnSpawnTab::CreateSP(this, &SProcedureEditorWidget::SpawnStatusWidgetTab))
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
			FNewMenuDelegate::CreateSP(this, &SProcedureEditorWidget::HandlePullDownWindowMenu),
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
					.Text(FText::FromString(TEXT("No procedure module found, please close this window!")))
					.ColorAndOpacity(FSlateColor(FLinearColor::Red))
				]
			]
		];
	}
}

void SProcedureEditorWidget::OnCreate()
{
	SEditorWidgetBase::OnCreate();

	OnBeginPIEHandle = FEditorDelegates::PostPIEStarted.AddRaw(this, &SProcedureEditorWidget::OnBeginPIE);

	OnEndPIEHandle = FEditorDelegates::EndPIE.AddRaw(this, &SProcedureEditorWidget::OnEndPIE);

	OnMapOpenedHandle = FEditorDelegates::OnMapOpened.AddRaw(this, &SProcedureEditorWidget::OnMapOpened);

	OnBlueprintCompiledHandle = GEditor->OnBlueprintCompiled().AddRaw(this, &SProcedureEditorWidget::OnBlueprintCompiled);
}

void SProcedureEditorWidget::OnReset()
{
	SEditorWidgetBase::OnReset();
}

void SProcedureEditorWidget::OnRefresh()
{
	ProcedureModule = UProcedureModule::Get(!bPreviewMode);
	if(ProcedureModule)
	{
		if(ListWidget)
		{
			ListWidget->ProcedureModule = ProcedureModule;
		}
		if(DetailsWidget)
		{
			DetailsWidget->ProcedureModule = ProcedureModule;
		}
		SEditorWidgetBase::OnRefresh();
	}
	else
	{
		Destroy();
	}
}

void SProcedureEditorWidget::OnDestroy()
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

void SProcedureEditorWidget::OnTabSpawned(const FName& TabIdentifier, const TSharedRef<SDockTab>& SpawnedTab)
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

void SProcedureEditorWidget::HandleTabManagerPersistLayout(const TSharedRef<FTabManager::FLayout>& LayoutToSave)
{
	if (FUnrealEdMisc::Get().IsSavingLayoutOnClosedAllowed())
	{
		FLayoutSaveRestore::SaveToConfig(GEditorLayoutIni, LayoutToSave);
	}
}

TSharedRef<SDockTab> SProcedureEditorWidget::SpawnToolbarWidgetTab(const FSpawnTabArgs& Args)
{
	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Label(LOCTEXT("ToolbarTab", "Toolbar"))
		.ShouldAutosize(true)
		[
			ToolbarWidget.ToSharedRef()
		];
	return SpawnedTab;
}

TSharedRef<SDockTab> SProcedureEditorWidget::SpawnListWidgetTab(const FSpawnTabArgs& Args)
{
	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Label(LOCTEXT("ListTab", "List"))
		.ShouldAutosize(false)
		[
			ListWidget.ToSharedRef()
		];
	return SpawnedTab;
}

TSharedRef<SDockTab> SProcedureEditorWidget::SpawnDetailsWidgetTab(const FSpawnTabArgs& Args)
{
	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Label(LOCTEXT("DetailsTab", "Details"))
		.ShouldAutosize(false)
		[
			DetailsWidget.ToSharedRef()
		];
	return SpawnedTab;
}

TSharedRef<SDockTab> SProcedureEditorWidget::SpawnStatusWidgetTab(const FSpawnTabArgs& Args)
{
	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Label(LOCTEXT("StatusTab", "Status"))
		.ShouldAutosize(true)
		[
			StatusWidget.ToSharedRef()
		];
	return SpawnedTab;
}

void SProcedureEditorWidget::HandlePullDownWindowMenu(FMenuBuilder& MenuBuilder)
{
	if (!TabManager.IsValid())
	{
		return;
	}

	TabManager->PopulateLocalTabSpawnerMenu(MenuBuilder);
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

#undef LOCTEXT_NAMESPACE

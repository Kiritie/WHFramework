// Fill out your copyright notice in the Description page of Project Settings.


#include "Main/Widget/SModuleEditorWidget.h"

#include "LevelEditorActions.h"
#include "SlateOptMacros.h"
#include "SPrimaryButton.h"
#include "Common/CommonStatics.h"
#include "Kismet2/SClassPickerDialog.h"
#include "Main/MainModule.h"
#include "Main/Widget/SModuleDetailsWidget.h"
#include "Main/Widget/SModuleListWidget.h"
#include "Main/Widget/SModuleStatusWidget.h"
#include "Main/Widget/SModuleToolbarWidget.h"
#include "Subsystems/EditorActorSubsystem.h"

#define LOCTEXT_NAMESPACE "ModuleEditorWidget"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

SModuleEditorWidget::SModuleEditorWidget()
{
	WidgetName = FName("ModuleEditorWidget");
	WidgetType = EEditorWidgetType::Main;

	MainModule = nullptr;

	bPreview = false;

	bNeedRebuild = false;
}

void SModuleEditorWidget::Construct(const FArguments& InArgs)
{
	SEditorWidgetBase::Construct(SEditorWidgetBase::FArguments());
}

void SModuleEditorWidget::OnCreate()
{
	SEditorWidgetBase::OnCreate();

	OnBeginPIEHandle = FEditorDelegates::PostPIEStarted.AddRaw(this, &SModuleEditorWidget::OnBeginPIE);

	OnEndPIEHandle = FEditorDelegates::EndPIE.AddRaw(this, &SModuleEditorWidget::OnEndPIE);

	OnMapChangeHandle = FEditorDelegates::MapChange.AddRaw(this, &SModuleEditorWidget::OnMapChanged);
	
	OnMapOpenedHandle = FEditorDelegates::OnMapOpened.AddRaw(this, &SModuleEditorWidget::OnMapOpened);

	OnBlueprintCompiledHandle = GEditor->OnBlueprintCompiled().AddRaw(this, &SModuleEditorWidget::OnBlueprintCompiled);
	
	MainModule = AMainModule::GetPtr(!UCommonStatics::IsPlaying() || !bPreview, true);

	if(MainModule)
	{
		SAssignNewEd(ListWidget, SModuleListWidget, SharedThis(this));

		SAssignNewEd(DetailsWidget, SModuleDetailsWidget, SharedThis(this));

		SAssignNewEd(StatusWidget, SModuleStatusWidget, SharedThis(this));

		SAssignNewEd(ToolbarWidget, SModuleToolbarWidget, SharedThis(this));

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
			LOCTEXT("FileMenuLabel", "File"),
			FText::GetEmpty(),
			FNewMenuDelegate::CreateSP(this, &SModuleEditorWidget::HandlePullDownFileMenu),
			"File"
		);
		
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
		
		SetIsPreview(UCommonStatics::IsPlaying());

		bNeedRebuild = false;
	}
	else
	{
		ChildSlot
		[
			SNew(SOverlay)

			+ SOverlay::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Fill)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Fill)
				.Padding(5.f)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("No main module found, please close this window or create one!")))
						.ColorAndOpacity(FSlateColor(FLinearColor::White))
					]
				]

				+ SVerticalBox::Slot()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Fill)
				.Padding(5.f)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					[
						SNew(SVerticalBox)

						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(SPrimaryButton)
							.Text(FText::FromString(TEXT("Create")))
							.OnClicked(this, &SModuleEditorWidget::OnCreateMainModuleButtonClicked)
						]
					]
				]
			]
		];

		bNeedRebuild = true;
	}
}

void SModuleEditorWidget::OnSave()
{
	SEditorWidgetBase::OnSave();

	FLevelEditorActionCallbacks::Save();
}

void SModuleEditorWidget::OnReset()
{
	SEditorWidgetBase::OnReset();
}

void SModuleEditorWidget::OnRefresh()
{
	MainModule = AMainModule::GetPtr(!UCommonStatics::IsPlaying() || !bPreview, true);
	if(MainModule)
	{
		if(bNeedRebuild)
		{
			Rebuild();
		}
		else
		{
			SEditorWidgetBase::OnRefresh();
		}
	}
	else
	{
		Rebuild();
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

	if(OnMapChangeHandle.IsValid())
	{
		FEditorDelegates::MapChange.Remove(OnMapChangeHandle);
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

void SModuleEditorWidget::OnBindCommands(const TSharedRef<FUICommandList>& InCommands)
{
	SEditorWidgetBase::OnBindCommands(InCommands);

	InCommands->MapAction(
		FModuleEditorCommands::Get().SaveModuleEditor,
		FExecuteAction::CreateSP(this, &SModuleEditorWidget::Save),
		FCanExecuteAction::CreateSP(this, &SModuleEditorWidget::CanSave)
	);
}

void SModuleEditorWidget::OnBeginPIE(bool bIsSimulating)
{
	if(!bIsSimulating)
	{
		SetIsPreview(true);
	}
}

void SModuleEditorWidget::OnEndPIE(bool bIsSimulating)
{
	if(!bIsSimulating)
	{
		SetIsPreview(false);
	}
}

void SModuleEditorWidget::OnMapOpened(const FString& Filename, bool bAsTemplate)
{
	Refresh();
}

void SModuleEditorWidget::OnMapChanged(uint32 MapChangeFlags)
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
			ToolbarWidget->TakeWidget()
		];
	return SpawnedTab;
}

TSharedRef<SDockTab> SModuleEditorWidget::SpawnListWidgetTab(const FSpawnTabArgs& Args)
{
	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Label(LOCTEXT("ListTab", "List"))
		.ShouldAutosize(false)
		[
			ListWidget->TakeWidget()
		];
	return SpawnedTab;
}

TSharedRef<SDockTab> SModuleEditorWidget::SpawnDetailsWidgetTab(const FSpawnTabArgs& Args)
{
	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Label(LOCTEXT("DetailsTab", "Details"))
		.ShouldAutosize(false)
		[
			DetailsWidget->TakeWidget()
		];
	return SpawnedTab;
}

TSharedRef<SDockTab> SModuleEditorWidget::SpawnStatusWidgetTab(const FSpawnTabArgs& Args)
{
	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Label(LOCTEXT("StatusTab", "Status"))
		.ShouldAutosize(true)
		[
			StatusWidget->TakeWidget()
		];
	return SpawnedTab;
}

void SModuleEditorWidget::HandlePullDownFileMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.AddMenuEntry(FModuleEditorCommands::Get().SaveModuleEditor);
}

void SModuleEditorWidget::HandlePullDownWindowMenu(FMenuBuilder& MenuBuilder)
{
	if (!TabManager.IsValid())
	{
		return;
	}

	TabManager->PopulateLocalTabSpawnerMenu(MenuBuilder);
}

FReply SModuleEditorWidget::OnCreateMainModuleButtonClicked()
{
	// Fill in options
	FClassViewerInitializationOptions Options;
	Options.Mode = EClassViewerMode::ClassPicker;
	Options.DisplayMode = EClassViewerDisplayMode::ListView;
	Options.NameTypeToDisplay = EClassViewerNameTypeToDisplay::DisplayName;
	
	const TSharedPtr<FClassViewerFilterBase> Filter = MakeShareable(new FClassViewerFilterBase);
	Filter->DisallowedClassFlags = CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists | CLASS_HideDropDown;
	Filter->AllowedChildrenOfClasses.Add(AMainModule::StaticClass());
	
	Options.ClassFilters.Add(Filter.ToSharedRef());

	Options.bExpandAllNodes = true;
	Options.bShowDefaultClasses = false;

	UClass* PickedClass = nullptr;

	const bool bPressedOk = SClassPickerDialog::PickClass(LOCTEXT("CreateMainModuleOptions", "Pick Class For MainModule"), Options, PickedClass, UObject::StaticClass());
	
	if(bPressedOk && PickedClass)
	{
		if(UEditorActorSubsystem* EditorActorSubsystem = GEditor->GetEditorSubsystem<UEditorActorSubsystem>())
		{
			EditorActorSubsystem->SpawnActorFromClass(PickedClass, FVector::ZeroVector, FRotator::ZeroRotator);

			Rebuild();
		}
	}
	
	return FReply::Handled();
}

void SModuleEditorWidget::TogglePreview()
{
	SetIsPreview(!bPreview);
}

void SModuleEditorWidget::SetIsPreview(bool bIsPreview)
{
	if(bPreview != bIsPreview)
	{
		bPreview = bIsPreview;
		Refresh();
		SetRenderOpacity(bPreview ? 0.8f : 1.f);
	}
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

#undef LOCTEXT_NAMESPACE

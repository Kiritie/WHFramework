// Fill out your copyright notice in the Description page of Project Settings.


#include "Main/Widget/SModuleEditorWidget.h"

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

	DefaultLayoutName = FName("ModuleEditor_Layout");

	MainModule = nullptr;

	bPreview = false;

	bNeedRebuild = false;
}

void SModuleEditorWidget::Construct(const FArguments& InArgs, const TSharedRef<SDockTab>& InNomadTab)
{
	SMainEditorWidgetBase::Construct(SMainEditorWidgetBase::FArguments(), InNomadTab);
}

void SModuleEditorWidget::OnCreate()
{
	SMainEditorWidgetBase::OnCreate();

	OnBeginPIEHandle = FEditorDelegates::PostPIEStarted.AddRaw(this, &SModuleEditorWidget::OnBeginPIE);

	OnEndPIEHandle = FEditorDelegates::EndPIE.AddRaw(this, &SModuleEditorWidget::OnEndPIE);

	OnMapChangeHandle = FEditorDelegates::MapChange.AddRaw(this, &SModuleEditorWidget::OnMapChanged);
	
	OnMapOpenedHandle = FEditorDelegates::OnMapOpened.AddRaw(this, &SModuleEditorWidget::OnMapOpened);

	OnBlueprintCompiledHandle = GEditor->OnBlueprintCompiled().AddRaw(this, &SModuleEditorWidget::OnBlueprintCompiled);
}

void SModuleEditorWidget::OnSave()
{
	SMainEditorWidgetBase::OnSave();
}

void SModuleEditorWidget::OnReset()
{
	SMainEditorWidgetBase::OnReset();
}

void SModuleEditorWidget::OnRefresh()
{
	MainModule = AMainModule::GetPtr(!bPreview, true);
	if(MainModule)
	{
		if(bNeedRebuild)
		{
			Rebuild();
		}
		else
		{
			SMainEditorWidgetBase::OnRefresh();
		}
	}
	else
	{
		Rebuild();
	}
}

void SModuleEditorWidget::OnDestroy()
{
	SMainEditorWidgetBase::OnDestroy();

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
	SMainEditorWidgetBase::OnBindCommands(InCommands);

	InCommands->MapAction(
		FModuleEditorCommands::Get().SaveModuleEditor,
		FExecuteAction::CreateSP(this, &SModuleEditorWidget::Save),
		FCanExecuteAction::CreateSP(this, &SModuleEditorWidget::CanSave)
	);
}

void SModuleEditorWidget::RegisterMenuBar(FMenuBarBuilder& InMenuBarBuilder)
{
	InMenuBarBuilder.AddPullDownMenu(
		LOCTEXT("FileMenuLabel", "File"),
		FText::GetEmpty(),
		FNewMenuDelegate::CreateSP(this, &SModuleEditorWidget::HandlePullDownFileMenu),
		"File"
	);
		
	InMenuBarBuilder.AddPullDownMenu(
		LOCTEXT("WindowMenuLabel", "Window"),
		FText::GetEmpty(),
		FNewMenuDelegate::CreateSP(this, &SModuleEditorWidget::HandlePullDownWindowMenu),
		"Window"
	);
}

void SModuleEditorWidget::RegisterTabSpawners()
{
	SAssignNewEdN(ListWidget, SModuleListWidget, SharedThis(this));

	SAssignNewEdN(DetailsWidget, SModuleDetailsWidget, SharedThis(this));

	SAssignNewEdN(StatusWidget, SModuleStatusWidget, SharedThis(this));

	SAssignNewEdN(ToolbarWidget, SModuleToolbarWidget, SharedThis(this));

	RegisterTabSpawner("Toolbar", FOnSpawnTab::CreateSP(this, &SModuleEditorWidget::SpawnToolbarWidgetTab))
		.SetDisplayName(LOCTEXT("ToolbarTab", "Toolbar"))
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Toolbar"));

	RegisterTabSpawner("List", FOnSpawnTab::CreateSP(this, &SModuleEditorWidget::SpawnListWidgetTab))
		.SetDisplayName(LOCTEXT("ListTab", "List"))
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Outliner"));

	RegisterTabSpawner("Details", FOnSpawnTab::CreateSP(this, &SModuleEditorWidget::SpawnDetailsWidgetTab))
		.SetDisplayName(LOCTEXT("DetailsTab", "Details"))
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));

	RegisterTabSpawner("Status", FOnSpawnTab::CreateSP(this, &SModuleEditorWidget::SpawnStatusWidgetTab))
		.SetDisplayName(LOCTEXT("StatusTab", "Status"))
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.StatsViewer"));
}

void SModuleEditorWidget::UnRegisterTabSpawners()
{
	UnRegisterTabSpawner("Toolbar");
	UnRegisterTabSpawner("List");
	UnRegisterTabSpawner("Details");
	UnRegisterTabSpawner("Status");
}

TSharedRef<FTabManager::FLayout> SModuleEditorWidget::CreateDefaultLayout()
{
	return FLayoutSaveRestore::LoadFromConfig(GEditorLayoutIni,
		SMainEditorWidgetBase::CreateDefaultLayout()
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
		   ));
}

TSharedRef<SWidget> SModuleEditorWidget::CreateMainWidget()
{
	MainModule = AMainModule::GetPtr(!bPreview, true);

	if(!MainModule)
	{
		bNeedRebuild = true;
		
		return SNew(SOverlay)
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
			];
	}

	bNeedRebuild = false;
	
	SetIsPreview(UCommonStatics::IsPlaying());

	return SMainEditorWidgetBase::CreateMainWidget();
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

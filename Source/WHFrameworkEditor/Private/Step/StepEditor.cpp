// Copyright Epic Games, Inc. All Rights Reserved.

#include "Step/StepEditor.h"

#include "Editor.h"
#include "ISettingsSection.h"
#include "Step/StepModule.h"
#include "Step/Base/StepAsset.h"
#include "Step/Base/StepBlueprint.h"
#include "Step/Blueprint/StepBlueprintActions.h"
#include "Step/Blueprint/StepGraphSchema.h"
#include "Step/Customization/StepDetailCustomization.h"
#include "Step/Widget/SStepDetailsWidget.h"
#include "Step/Widget/SStepListWidget.h"
#include "Step/Widget/SStepStatusWidget.h"

#define LOCTEXT_NAMESPACE "FStepEditor"

//////////////////////////////////////////////////////////////////////////
/// StepEditorModule
IMPLEMENTATION_EDITOR_MODULE(FStepEditorModule)

FStepEditorModule::FStepEditorModule()
{
	AppIdentifier = FName("StepEditorApp");
}

void FStepEditorModule::StartupModule()
{
	FStepEditorCommands::Register();
}

void FStepEditorModule::ShutdownModule()
{
	FStepEditorCommands::Unregister();
}

void FStepEditorModule::RegisterSettings(ISettingsModule* SettingsModule)
{
	GStepEditorIni = GConfig->GetDestIniFilename(TEXT("StepEditor"), *UGameplayStatics::GetPlatformName(), *FPaths::GeneratedConfigDir());
	const ISettingsSectionPtr SettingsSection = SettingsModule->RegisterSettings(FName("Project"), FName("WHFramework"), FName("Step Editor"), FText::FromString(TEXT("Step Editor")), FText::FromString(TEXT("Configure the Step editor plugin")), GetMutableDefault<UStepEditorSettings>());
	if(SettingsSection.IsValid())
	{
		SettingsSection->OnModified().BindRaw(this, &FStepEditorModule::HandleSettingsSaved);
	}
}

void FStepEditorModule::UnRegisterSettings(ISettingsModule* SettingsModule)
{
	SettingsModule->UnregisterSettings(FName("Project"), FName("Plugins"), FName("Step Editor"));
}

bool FStepEditorModule::HandleSettingsSaved()
{
	UStepEditorSettings* StepEditorSetting = GetMutableDefault<UStepEditorSettings>();
	StepEditorSetting->SaveConfig();
	return true;
}

void FStepEditorModule::RegisterCommands(const TSharedPtr<FUICommandList>& Commands)
{
	Commands->MapAction(
		FStepEditorCommands::Get().OpenStepEditorWindow,
		FExecuteAction::CreateRaw(this, &FStepEditorModule::OnClickedStepEditorButton),
		FCanExecuteAction());
}

void FStepEditorModule::RegisterMenus(const TSharedPtr<FUICommandList>& Commands)
{
	AddWindowMenu(FStepEditorCommands::Get().OpenStepEditorWindow, Commands);
	
	AddToolbarMenu(FStepEditorCommands::Get().OpenStepEditorWindow, Commands);
}

void FStepEditorModule::RegisterAssetTypeAction(IAssetTools& AssetTools, EAssetTypeCategories::Type AssetCategory, TArray<TSharedPtr<IAssetTypeActions>>& AssetTypeActions)
{
	const TSharedRef<IAssetTypeActions> AssetAction = MakeShareable(new FStepBlueprintActions(AssetCategory));
	AssetTools.RegisterAssetTypeActions(AssetAction);
	AssetTypeActions.Add(AssetAction);
}

void FStepEditorModule::RegisterCustomClassLayout(FPropertyEditorModule& PropertyEditor)
{
	FEditorModuleBase::RegisterCustomClassLayout(PropertyEditor);
	
	PropertyEditor.RegisterCustomClassLayout(FName("StepBase"), FOnGetDetailCustomizationInstance::CreateStatic(&FStepDetailCustomization::MakeInstance));
}

void FStepEditorModule::UnRegisterCustomClassLayout(FPropertyEditorModule& PropertyEditor)
{
	PropertyEditor.UnregisterCustomClassLayout(FName("StepBase"));
}

TSharedRef<FStepEditor> FStepEditorModule::CreateStepEditor(const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, UStepAsset* Step)
{
	TSharedRef< FStepEditor > NewStepEditor( new FStepEditor() );
	NewStepEditor->InitAssetEditorBase( Mode, InitToolkitHost, Step );
	return NewStepEditor;
}

void FStepEditorModule::OnClickedStepEditorButton()
{
	if(const UStepModule* StepModule = UStepModule::GetPtr(true))
	{
		if(StepModule->GetDefaultAsset())
		{
			GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(StepModule->GetDefaultAsset());
		}
	}
}

//////////////////////////////////////////////////////////////////////////
/// StepEditor
FStepEditor::FStepEditor()
{
	ToolkitFName = FName("StepEditor");
	BaseToolkitName =  LOCTEXT("AppLabel", "Step Editor");
	MenuCategory = LOCTEXT("StepEditor", "Step Editor");
	DefaultLayoutName = FName("StepEditor_Layout");
	WorldCentricTabPrefix =  LOCTEXT("WorldCentricTabPrefix", "Step ").ToString();
	WorldCentricTabColorScale =  FLinearColor( 0.0f, 0.0f, 0.2f, 0.5f );
}

FStepEditor::~FStepEditor()
{
	
}

void FStepEditor::InitAssetEditorBase(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UObject* Asset)
{
	FAssetEditorBase::InitAssetEditorBase(Mode, InitToolkitHost, Asset);
}

void FStepEditor::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorBase::RegisterTabSpawners(InTabManager);

	SAssignNewEd(ListWidget, SStepListWidget, nullptr)
		.StepEditor(SharedThis(this));

	SAssignNewEd(DetailsWidget, SStepDetailsWidget, nullptr)
		.StepEditor(SharedThis(this));

	SAssignNewEd(StatusWidget, SStepStatusWidget, nullptr)
		.StepEditor(SharedThis(this));

	RegisterTrackedTabSpawner(InTabManager, "List", FOnSpawnTab::CreateSP(this, &FStepEditor::SpawnListWidgetTab))
		.SetDisplayName(LOCTEXT("ListTab", "List"))
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Outliner"));

	RegisterTrackedTabSpawner(InTabManager, "Details", FOnSpawnTab::CreateSP(this, &FStepEditor::SpawnDetailsWidgetTab))
		.SetDisplayName(LOCTEXT("DetailsTab", "Details"))
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));

	RegisterTrackedTabSpawner(InTabManager, "Status", FOnSpawnTab::CreateSP(this, &FStepEditor::SpawnStatusWidgetTab))
		.SetDisplayName(LOCTEXT("StatusTab", "Status"))
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.StatsViewer"));
}

void FStepEditor::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner("List");
	InTabManager->UnregisterTabSpawner("Details");
	InTabManager->UnregisterTabSpawner("Status");
}

TSharedRef<FTabManager::FLayout> FStepEditor::CreateDefaultLayout()
{
	const TSharedRef<FTabManager::FLayout> DefaultLayout = FAssetEditorBase::CreateDefaultLayout();

	DefaultLayout->AddArea
	(
		FTabManager::NewPrimaryArea()
		->SetOrientation(Orient_Vertical)
		->Split
		(
			// Main application area
			FTabManager::NewSplitter()
			->SetOrientation(Orient_Horizontal)
			->SetSizeCoefficient(0.9f)
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

	return DefaultLayout;
}

void FStepEditor::ExtendToolbar(FToolBarBuilder& ToolbarBuilder)
{
	ToolbarBuilder.BeginSection("List");
	{
		ToolbarBuilder.AddToolBarButton(
			FUIAction(
				FExecuteAction::CreateRaw(this, &FStepEditor::OnDefaultsToggled),
				FCanExecuteAction(),
				FGetActionCheckState::CreateLambda([this](){
					return ListWidget->bDefaults ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
			),
			NAME_None,
			FText::FromString(TEXT("Defaults")),
			FText::FromString(TEXT("Toggle Defaults")),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "FullBlueprintEditor.EditGlobalOptions"),
			EUserInterfaceActionType::ToggleButton
		);
		ToolbarBuilder.AddToolBarButton(
			FUIAction(
				FExecuteAction::CreateRaw(this, &FStepEditor::OnEditingToggled),
				FCanExecuteAction(),
				FGetActionCheckState::CreateLambda([this](){
					return ListWidget->bEditing ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
			),
			NAME_None,
			FText::FromString(TEXT("Editing")),
			FText::FromString(TEXT("Toggle Editing")),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"),
			EUserInterfaceActionType::ToggleButton
		);
	}
	ToolbarBuilder.EndSection();
}

TSharedRef<SDockTab> FStepEditor::SpawnListWidgetTab(const FSpawnTabArgs& Args)
{
	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Label(LOCTEXT("ListTab", "List"))
		.ShouldAutosize(false)
		[
			ListWidget->TakeWidget()
		];
	return SpawnedTab;
}

TSharedRef<SDockTab> FStepEditor::SpawnDetailsWidgetTab(const FSpawnTabArgs& Args)
{
	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Label(LOCTEXT("DetailsTab", "Details"))
		.ShouldAutosize(false)
		[
			DetailsWidget->TakeWidget()
		];
	return SpawnedTab;
}

TSharedRef<SDockTab> FStepEditor::SpawnStatusWidgetTab(const FSpawnTabArgs& Args)
{
	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Label(LOCTEXT("StatusTab", "Status"))
		.ShouldAutosize(true)
		[
			StatusWidget->TakeWidget()
		];
	return SpawnedTab;
}

void FStepEditor::PostUndo(bool bSuccess)
{
	FAssetEditorBase::PostUndo(bSuccess);
}

void FStepEditor::PostRedo(bool bSuccess)
{
	FAssetEditorBase::PostRedo(bSuccess);
}

FEditorModuleBase* FStepEditor::GetEditorModule() const
{
	return &FStepEditorModule::Get();
}

void FStepEditor::OnBlueprintCompiled()
{
	FAssetEditorBase::OnBlueprintCompiled();
}

void FStepEditor::OnDefaultsToggled()
{
	ListWidget->ToggleDefaults();
}

void FStepEditor::OnEditingToggled()
{
	ListWidget->ToggleEditing();
}

//////////////////////////////////////////////////////////////////////////
/// StepBlueprintEditor
FStepBlueprintEditor::FStepBlueprintEditor()
{
	BlueprintClass = UStepBlueprint::StaticClass();
	GraphSchemaClass = UStepGraphSchema::StaticClass();
	
	ToolkitFName = FName("StepBlueprintEditor");
	BaseToolkitName = LOCTEXT("StepBlueprintEditorAppLabel", "Step Blueprint Editor");
	ToolkitNameFormat = LOCTEXT("StepBlueprintEditorToolkitName", "{ObjectName}{DirtyState}");

	WorldCentricTabPrefix = TEXT("StepBlueprintEditor");
	WorldCentricTabColorScale = FLinearColor::White;
}

#undef LOCTEXT_NAMESPACE

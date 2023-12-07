// Copyright Epic Games, Inc. All Rights Reserved.

#include "Task/TaskEditor.h"

#include "Editor.h"
#include "ISettingsSection.h"
#include "Task/TaskModule.h"
#include "Task/Base/TaskAsset.h"
#include "Task/Base/TaskBlueprint.h"
#include "Task/Blueprint/TaskBlueprintActions.h"
#include "Task/Blueprint/TaskGraphSchema.h"
#include "Task/Customization/TaskDetailCustomization.h"
#include "Task/Widget/STaskDetailsWidget.h"
#include "Task/Widget/STaskListWidget.h"
#include "Task/Widget/STaskStatusWidget.h"

#define LOCTEXT_NAMESPACE "FTaskEditor"

//////////////////////////////////////////////////////////////////////////
/// TaskEditorModule
IMPLEMENTATION_EDITOR_MODULE(FTaskEditorModule)

FTaskEditorModule::FTaskEditorModule()
{
	AppIdentifier = FName("TaskEditorApp");
}

void FTaskEditorModule::StartupModule()
{
	FTaskEditorCommands::Register();
}

void FTaskEditorModule::ShutdownModule()
{
	FTaskEditorCommands::Unregister();
}

void FTaskEditorModule::RegisterSettings(ISettingsModule* SettingsModule)
{
	GTaskEditorIni = GConfig->GetDestIniFilename(TEXT("TaskEditor"), *UGameplayStatics::GetPlatformName(), *FPaths::GeneratedConfigDir());
	const ISettingsSectionPtr SettingsSection = SettingsModule->RegisterSettings(FName("Project"), FName("WHFramework"), FName("Task Editor"), FText::FromString(TEXT("Task Editor")), FText::FromString(TEXT("Configure the Task editor plugin")), GetMutableDefault<UTaskEditorSettings>());
	if(SettingsSection.IsValid())
	{
		SettingsSection->OnModified().BindRaw(this, &FTaskEditorModule::HandleSettingsSaved);
	}
}

void FTaskEditorModule::UnRegisterSettings(ISettingsModule* SettingsModule)
{
	SettingsModule->UnregisterSettings(FName("Project"), FName("Plugins"), FName("Task Editor"));
}

bool FTaskEditorModule::HandleSettingsSaved()
{
	UTaskEditorSettings* TaskEditorSetting = GetMutableDefault<UTaskEditorSettings>();
	TaskEditorSetting->SaveConfig();
	return true;
}

void FTaskEditorModule::RegisterCommands(const TSharedPtr<FUICommandList>& Commands)
{
	Commands->MapAction(
		FTaskEditorCommands::Get().OpenTaskEditorWindow,
		FExecuteAction::CreateRaw(this, &FTaskEditorModule::OnClickedTaskEditorButton),
		FCanExecuteAction());
}

void FTaskEditorModule::RegisterMenus(const TSharedPtr<FUICommandList>& Commands)
{
	AddWindowMenu(FTaskEditorCommands::Get().OpenTaskEditorWindow, Commands);
	
	AddToolbarMenu(FTaskEditorCommands::Get().OpenTaskEditorWindow, Commands);
}

void FTaskEditorModule::RegisterAssetTypeAction(IAssetTools& AssetTools, EAssetTypeCategories::Type AssetCategory, TArray<TSharedPtr<IAssetTypeActions>>& AssetTypeActions)
{
	const TSharedRef<IAssetTypeActions> AssetAction = MakeShareable(new FTaskBlueprintActions(AssetCategory));
	AssetTools.RegisterAssetTypeActions(AssetAction);
	AssetTypeActions.Add(AssetAction);
}

void FTaskEditorModule::RegisterCustomClassLayout(FPropertyEditorModule& PropertyEditor)
{
	FEditorModuleBase::RegisterCustomClassLayout(PropertyEditor);
	
	PropertyEditor.RegisterCustomClassLayout(FName("TaskBase"), FOnGetDetailCustomizationInstance::CreateStatic(&FTaskDetailCustomization::MakeInstance));
}

void FTaskEditorModule::UnRegisterCustomClassLayout(FPropertyEditorModule& PropertyEditor)
{
	PropertyEditor.UnregisterCustomClassLayout(FName("TaskBase"));
}

TSharedRef<FTaskEditor> FTaskEditorModule::CreateTaskEditor(const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, UTaskAsset* Task)
{
	TSharedRef< FTaskEditor > NewTaskEditor( new FTaskEditor() );
	NewTaskEditor->InitAssetEditorBase( Mode, InitToolkitHost, Task );
	return NewTaskEditor;
}

void FTaskEditorModule::OnClickedTaskEditorButton()
{
	if(const UTaskModule* TaskModule = UTaskModule::GetPtr(true))
	{
		if(TaskModule->GetDefaultAsset())
		{
			GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(TaskModule->GetDefaultAsset());
		}
	}
}

//////////////////////////////////////////////////////////////////////////
/// TaskEditor
FTaskEditor::FTaskEditor()
{
	ToolkitFName = FName("TaskEditor");
	BaseToolkitName =  LOCTEXT("AppLabel", "Task Editor");
	MenuCategory = LOCTEXT("TaskEditor", "Task Editor");
	DefaultLayoutName = FName("TaskEditor_Layout");
	WorldCentricTabPrefix =  LOCTEXT("WorldCentricTabPrefix", "Task ").ToString();
	WorldCentricTabColorScale =  FLinearColor( 0.0f, 0.0f, 0.2f, 0.5f );
}

FTaskEditor::~FTaskEditor()
{
	
}

void FTaskEditor::InitAssetEditorBase(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UObject* Asset)
{
	FAssetEditorBase::InitAssetEditorBase(Mode, InitToolkitHost, Asset);
}

void FTaskEditor::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorBase::RegisterTabSpawners(InTabManager);

	SAssignNewEd(ListWidget, STaskListWidget, nullptr)
		.TaskEditor(SharedThis(this));

	SAssignNewEd(DetailsWidget, STaskDetailsWidget, nullptr)
		.TaskEditor(SharedThis(this));

	SAssignNewEd(StatusWidget, STaskStatusWidget, nullptr)
		.TaskEditor(SharedThis(this));

	RegisterTrackedTabSpawner(InTabManager, "List", FOnSpawnTab::CreateSP(this, &FTaskEditor::SpawnListWidgetTab))
		.SetDisplayName(LOCTEXT("ListTab", "List"))
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Outliner"));

	RegisterTrackedTabSpawner(InTabManager, "Details", FOnSpawnTab::CreateSP(this, &FTaskEditor::SpawnDetailsWidgetTab))
		.SetDisplayName(LOCTEXT("DetailsTab", "Details"))
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));

	RegisterTrackedTabSpawner(InTabManager, "Status", FOnSpawnTab::CreateSP(this, &FTaskEditor::SpawnStatusWidgetTab))
		.SetDisplayName(LOCTEXT("StatusTab", "Status"))
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.StatsViewer"));
}

void FTaskEditor::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner("List");
	InTabManager->UnregisterTabSpawner("Details");
	InTabManager->UnregisterTabSpawner("Status");
}

TSharedRef<FTabManager::FLayout> FTaskEditor::CreateDefaultLayout()
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

void FTaskEditor::ExtendToolbar(FToolBarBuilder& ToolbarBuilder)
{
	ToolbarBuilder.BeginSection("List");
	{
		ToolbarBuilder.AddToolBarButton(
			FUIAction(
				FExecuteAction::CreateRaw(this, &FTaskEditor::OnDefaultsToggled),
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
				FExecuteAction::CreateRaw(this, &FTaskEditor::OnEditingToggled),
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

TSharedRef<SDockTab> FTaskEditor::SpawnListWidgetTab(const FSpawnTabArgs& Args)
{
	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Label(LOCTEXT("ListTab", "List"))
		.ShouldAutosize(false)
		[
			ListWidget->TakeWidget()
		];
	return SpawnedTab;
}

TSharedRef<SDockTab> FTaskEditor::SpawnDetailsWidgetTab(const FSpawnTabArgs& Args)
{
	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Label(LOCTEXT("DetailsTab", "Details"))
		.ShouldAutosize(false)
		[
			DetailsWidget->TakeWidget()
		];
	return SpawnedTab;
}

TSharedRef<SDockTab> FTaskEditor::SpawnStatusWidgetTab(const FSpawnTabArgs& Args)
{
	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Label(LOCTEXT("StatusTab", "Status"))
		.ShouldAutosize(true)
		[
			StatusWidget->TakeWidget()
		];
	return SpawnedTab;
}

void FTaskEditor::PostUndo(bool bSuccess)
{
	FAssetEditorBase::PostUndo(bSuccess);
}

void FTaskEditor::PostRedo(bool bSuccess)
{
	FAssetEditorBase::PostRedo(bSuccess);
}

FEditorModuleBase* FTaskEditor::GetEditorModule() const
{
	return &FTaskEditorModule::Get();
}

void FTaskEditor::OnBlueprintCompiled()
{
	FAssetEditorBase::OnBlueprintCompiled();
}

void FTaskEditor::OnDefaultsToggled()
{
	ListWidget->ToggleDefaults();
}

void FTaskEditor::OnEditingToggled()
{
	ListWidget->ToggleEditing();
}

//////////////////////////////////////////////////////////////////////////
/// TaskBlueprintEditor
FTaskBlueprintEditor::FTaskBlueprintEditor()
{
	BlueprintClass = UTaskBlueprint::StaticClass();
	GraphSchemaClass = UTaskGraphSchema::StaticClass();
	
	ToolkitFName = FName("TaskBlueprintEditor");
	BaseToolkitName = LOCTEXT("TaskBlueprintEditorAppLabel", "Task Blueprint Editor");
	ToolkitNameFormat = LOCTEXT("TaskBlueprintEditorToolkitName", "{ObjectName}{DirtyState}");

	WorldCentricTabPrefix = TEXT("TaskBlueprintEditor");
	WorldCentricTabColorScale = FLinearColor::White;
}

#undef LOCTEXT_NAMESPACE

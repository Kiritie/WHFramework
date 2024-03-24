// Copyright Epic Games, Inc. All Rights Reserved.

#include "Procedure/ProcedureEditor.h"

#include "Editor.h"
#include "ISettingsSection.h"
#include "WHFrameworkSlateTypes.h"
#include "Procedure/ProcedureModule.h"
#include "Procedure/Base/ProcedureAsset.h"
#include "Procedure/Base/ProcedureBlueprint.h"
#include "Procedure/Blueprint/ProcedureBlueprintActions.h"
#include "Procedure/Blueprint/ProcedureGraphSchema.h"
#include "Procedure/Customization/ProcedureDetailCustomization.h"
#include "Procedure/Widget/SProcedureDetailsWidget.h"
#include "Procedure/Widget/SProcedureListWidget.h"
#include "Procedure/Widget/SProcedureStatusWidget.h"

#define LOCTEXT_NAMESPACE "FProcedureEditor"

//////////////////////////////////////////////////////////////////////////
/// ProcedureEditorModule
IMPLEMENTATION_EDITOR_MODULE(FProcedureEditorModule)

FProcedureEditorModule::FProcedureEditorModule()
{
	AppIdentifier = FName("ProcedureEditorApp");
}

void FProcedureEditorModule::StartupModule()
{
	FProcedureEditorCommands::Register();
}

void FProcedureEditorModule::ShutdownModule()
{
	FProcedureEditorCommands::Unregister();
}

void FProcedureEditorModule::RegisterSettings(ISettingsModule* SettingsModule)
{
	GProcedureEditorIni = GConfig->GetDestIniFilename(TEXT("ProcedureEditor"), *UGameplayStatics::GetPlatformName(), *FPaths::GeneratedConfigDir());
	const ISettingsSectionPtr SettingsSection = SettingsModule->RegisterSettings(FName("Project"), FName("WHFramework"), FName("Procedure Editor"), FText::FromString(TEXT("Procedure Editor")), FText::FromString(TEXT("Configure the procedure editor plugin")), GetMutableDefault<UProcedureEditorSettings>());
	if(SettingsSection.IsValid())
	{
		SettingsSection->OnModified().BindRaw(this, &FProcedureEditorModule::HandleSettingsSaved);
	}
}

void FProcedureEditorModule::UnRegisterSettings(ISettingsModule* SettingsModule)
{
	SettingsModule->UnregisterSettings(FName("Project"), FName("Plugins"), FName("Procedure Editor"));
}

bool FProcedureEditorModule::HandleSettingsSaved()
{
	UProcedureEditorSettings* ProcedureEditorSetting = GetMutableDefault<UProcedureEditorSettings>();
	ProcedureEditorSetting->SaveConfig();
	return true;
}

void FProcedureEditorModule::RegisterCommands(const TSharedPtr<FUICommandList>& Commands)
{
	Commands->MapAction(
		FProcedureEditorCommands::Get().OpenProcedureEditorWindow,
		FExecuteAction::CreateRaw(this, &FProcedureEditorModule::OnClickedProcedureEditorButton),
		FCanExecuteAction());
}

void FProcedureEditorModule::RegisterMenus(const TSharedPtr<FUICommandList>& Commands)
{
	AddWindowMenu(FProcedureEditorCommands::Get().OpenProcedureEditorWindow, Commands);
	
	AddToolbarMenu(FProcedureEditorCommands::Get().OpenProcedureEditorWindow, Commands);
}

void FProcedureEditorModule::RegisterAssetTypeAction(IAssetTools& AssetTools, EAssetTypeCategories::Type AssetCategory, TArray<TSharedPtr<IAssetTypeActions>>& AssetTypeActions)
{
	const TSharedRef<IAssetTypeActions> AssetAction = MakeShareable(new FProcedureBlueprintActions(AssetCategory));
	AssetTools.RegisterAssetTypeActions(AssetAction);
	AssetTypeActions.Add(AssetAction);
}

void FProcedureEditorModule::RegisterCustomClassLayout(FPropertyEditorModule& PropertyEditor)
{
	FEditorModuleBase::RegisterCustomClassLayout(PropertyEditor);
	
	PropertyEditor.RegisterCustomClassLayout(FName("ProcedureBase"), FOnGetDetailCustomizationInstance::CreateStatic(&FProcedureDetailCustomization::MakeInstance));
}

void FProcedureEditorModule::UnRegisterCustomClassLayout(FPropertyEditorModule& PropertyEditor)
{
	PropertyEditor.UnregisterCustomClassLayout(FName("ProcedureBase"));
}

TSharedRef<FProcedureEditor> FProcedureEditorModule::CreateProcedureEditor(const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, UProcedureAsset* Procedure)
{
	TSharedRef< FProcedureEditor > NewProcedureEditor( new FProcedureEditor() );
	NewProcedureEditor->InitAssetEditorBase( Mode, InitToolkitHost, Procedure );
	return NewProcedureEditor;
}

void FProcedureEditorModule::OnClickedProcedureEditorButton()
{
	if(const UProcedureModule* ProcedureModule = UProcedureModule::GetPtr(!UCommonStatics::IsPlaying()))
	{
		if(UProcedureAsset* ProcedureAsset = !UCommonStatics::IsPlaying() ? ProcedureModule->GetDefaultAsset() : ProcedureModule->GetCurrentAsset())
		{
			GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(ProcedureAsset);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
/// ProcedureEditor
FProcedureEditor::FProcedureEditor()
{
	ToolkitFName = FName("ProcedureEditor");
	BaseToolkitName =  LOCTEXT("AppLabel", "Procedure Editor");
	MenuCategory = LOCTEXT("ProcedureEditor", "Procedure Editor");
	DefaultLayoutName = FName("ProcedureEditor_Layout");
	WorldCentricTabPrefix =  LOCTEXT("WorldCentricTabPrefix", "Procedure ").ToString();
	WorldCentricTabColorScale =  FLinearColor( 0.0f, 0.0f, 0.2f, 0.5f );
}

FProcedureEditor::~FProcedureEditor()
{
	
}

void FProcedureEditor::InitAssetEditorBase(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UObject* Asset)
{
	FAssetEditorBase::InitAssetEditorBase(Mode, InitToolkitHost, Asset);
}

void FProcedureEditor::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorBase::RegisterTabSpawners(InTabManager);

	SAssignNewEd(ListWidget, SProcedureListWidget, true)
		.ProcedureEditor(SharedThis(this));

	SAssignNewEd(DetailsWidget, SProcedureDetailsWidget, true)
		.ProcedureEditor(SharedThis(this));

	SAssignNewEd(StatusWidget, SProcedureStatusWidget, true)
		.ProcedureEditor(SharedThis(this));

	RegisterTrackedTabSpawner(InTabManager, "List", FOnSpawnTab::CreateSP(this, &FProcedureEditor::SpawnListWidgetTab))
		.SetDisplayName(LOCTEXT("ListTab", "List"))
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Outliner"));

	RegisterTrackedTabSpawner(InTabManager, "Details", FOnSpawnTab::CreateSP(this, &FProcedureEditor::SpawnDetailsWidgetTab))
		.SetDisplayName(LOCTEXT("DetailsTab", "Details"))
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));

	RegisterTrackedTabSpawner(InTabManager, "Status", FOnSpawnTab::CreateSP(this, &FProcedureEditor::SpawnStatusWidgetTab))
		.SetDisplayName(LOCTEXT("StatusTab", "Status"))
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.StatsViewer"));
}

void FProcedureEditor::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner("List");
	InTabManager->UnregisterTabSpawner("Details");
	InTabManager->UnregisterTabSpawner("Status");
}

TSharedRef<FTabManager::FLayout> FProcedureEditor::CreateDefaultLayout()
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

void FProcedureEditor::ExtendToolbar(FToolBarBuilder& ToolbarBuilder)
{
	ToolbarBuilder.BeginSection("List");
	{
		ToolbarBuilder.AddToolBarButton(
			FUIAction(
				FExecuteAction::CreateRaw(this, &FProcedureEditor::OnDefaultsToggled),
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
				FExecuteAction::CreateRaw(this, &FProcedureEditor::OnEditingToggled),
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

TSharedRef<SDockTab> FProcedureEditor::SpawnListWidgetTab(const FSpawnTabArgs& Args)
{
	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Label(LOCTEXT("ListTab", "List"))
		.ShouldAutosize(false)
		[
			ListWidget->TakeWidget()
		];
	return SpawnedTab;
}

TSharedRef<SDockTab> FProcedureEditor::SpawnDetailsWidgetTab(const FSpawnTabArgs& Args)
{
	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Label(LOCTEXT("DetailsTab", "Details"))
		.ShouldAutosize(false)
		[
			DetailsWidget->TakeWidget()
		];
	return SpawnedTab;
}

TSharedRef<SDockTab> FProcedureEditor::SpawnStatusWidgetTab(const FSpawnTabArgs& Args)
{
	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Label(LOCTEXT("StatusTab", "Status"))
		.ShouldAutosize(true)
		[
			StatusWidget->TakeWidget()
		];
	return SpawnedTab;
}

void FProcedureEditor::PostUndo(bool bSuccess)
{
	FAssetEditorBase::PostUndo(bSuccess);
}

void FProcedureEditor::PostRedo(bool bSuccess)
{
	FAssetEditorBase::PostRedo(bSuccess);
}

FEditorModuleBase* FProcedureEditor::GetEditorModule() const
{
	return &FProcedureEditorModule::Get();
}

void FProcedureEditor::OnBlueprintCompiled()
{
	FAssetEditorBase::OnBlueprintCompiled();

	ListWidget->Refresh();
}

void FProcedureEditor::OnDefaultsToggled()
{
	ListWidget->ToggleDefaults();
}

void FProcedureEditor::OnEditingToggled()
{
	ListWidget->ToggleEditing();
}

//////////////////////////////////////////////////////////////////////////
/// ProcedureBlueprintEditor
FProcedureBlueprintEditor::FProcedureBlueprintEditor()
{
	BlueprintClass = UProcedureBlueprint::StaticClass();
	GraphSchemaClass = UProcedureGraphSchema::StaticClass();
	
	ToolkitFName = FName("ProcedureBlueprintEditor");
	BaseToolkitName = LOCTEXT("ProcedureBlueprintEditorAppLabel", "Procedure Blueprint Editor");
	ToolkitNameFormat = LOCTEXT("ProcedureBlueprintEditorToolkitName", "{ObjectName}{DirtyState}");

	WorldCentricTabPrefix = TEXT("ProcedureBlueprintEditor");
	WorldCentricTabColorScale = FLinearColor::White;
}

#undef LOCTEXT_NAMESPACE

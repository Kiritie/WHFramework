// Copyright Epic Games, Inc. All Rights Reserved.

#include "Main/MainEditor.h"

#include "IAssetTools.h"
#include "ISettingsSection.h"
#include "WHFrameworkSlateStatics.h"
#include "Main/MainEditorTypes.h"
#include "Main/Base/ModuleBlueprint.h"
#include "Main/Blueprint/ModuleBlueprintActions.h"
#include "Main/Blueprint/ModuleGraphSchema.h"
#include "Main/Customization/MainModuleDetailCustomization.h"
#include "Main/Widget/SModuleEditorWidget.h"

#define LOCTEXT_NAMESPACE "FModuleEditor"

//////////////////////////////////////////////////////////////////////////
/// MainEditorModule
IMPLEMENTATION_EDITOR_MODULE(FMainEditorModule)

static const FName ModuleEditorTabName("ModuleEditor");

FMainEditorModule::FMainEditorModule()
{
	AppIdentifier = FName("ModuleEditorApp");
}

void FMainEditorModule::StartupModule()
{
	FModuleEditorCommands::Register();

	// Register tabs
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(ModuleEditorTabName, FOnSpawnTab::CreateRaw(this, &FMainEditorModule::OnSpawnModuleEditorTab))
		.SetDisplayName(LOCTEXT("FModuleEditorTabTitle", "Module Editor"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);
}

void FMainEditorModule::ShutdownModule()
{
	FModuleEditorCommands::Unregister();
}

void FMainEditorModule::RegisterSettings(ISettingsModule* SettingsModule)
{
	GModuleEditorIni = GConfig->GetDestIniFilename(TEXT("ModuleEditor"), *UGameplayStatics::GetPlatformName(), *FPaths::GeneratedConfigDir());
	const ISettingsSectionPtr SettingsSection = SettingsModule->RegisterSettings(FName("Project"), FName("WHFramework"), FName("Module Editor"), FText::FromString(TEXT("Module Editor")), FText::FromString(TEXT("Configure the Module editor plugin")), GetMutableDefault<UModuleEditorSettings>());
	if(SettingsSection.IsValid())
	{
		SettingsSection->OnModified().BindRaw(this, &FMainEditorModule::HandleSettingsSaved);
	}
}

void FMainEditorModule::UnRegisterSettings(ISettingsModule* SettingsModule)
{
	SettingsModule->UnregisterSettings(FName("Project"), FName("Plugins"), FName("Module Editor"));
}

bool FMainEditorModule::HandleSettingsSaved()
{
	UModuleEditorSettings* ModuleEditorSetting = GetMutableDefault<UModuleEditorSettings>();
	ModuleEditorSetting->SaveConfig();
	return true;
}

void FMainEditorModule::RegisterCommands(const TSharedPtr<FUICommandList>& Commands)
{
	Commands->MapAction(
		FModuleEditorCommands::Get().OpenModuleEditorWindow,
		FExecuteAction::CreateRaw(this, &FMainEditorModule::OpenModuleEditor),
		FCanExecuteAction());
}

void FMainEditorModule::RegisterMenus(const TSharedPtr<FUICommandList>& Commands)
{
	AddWindowMenu(FModuleEditorCommands::Get().OpenModuleEditorWindow, Commands);
	
	AddToolbarMenu(FModuleEditorCommands::Get().OpenModuleEditorWindow, Commands);
}

void FMainEditorModule::RegisterAssetTypeAction(IAssetTools& AssetTools, EAssetTypeCategories::Type AssetCategory, TArray<TSharedPtr<IAssetTypeActions>>& AssetTypeActions)
{
	const TSharedRef<IAssetTypeActions> AssetAction = MakeShareable(new FModuleBlueprintActions(AssetCategory));
	AssetTools.RegisterAssetTypeActions(AssetAction);
	AssetTypeActions.Add(AssetAction);
}

void FMainEditorModule::RegisterCustomClassLayout(FPropertyEditorModule& PropertyEditor)
{
	FEditorModuleBase::RegisterCustomClassLayout(PropertyEditor);
	
	PropertyEditor.RegisterCustomClassLayout(FName("MainModule"), FOnGetDetailCustomizationInstance::CreateStatic(&FMainModuleDetailCustomization::MakeInstance));
}

void FMainEditorModule::UnRegisterCustomClassLayout(FPropertyEditorModule& PropertyEditor)
{
	PropertyEditor.UnregisterCustomClassLayout(FName("MainModule"));
}

void FMainEditorModule::OpenModuleEditor()
{
	FGlobalTabmanager::Get()->TryInvokeTab(FName("ModuleEditor"));
}

TSharedRef<SDockTab> FMainEditorModule::OnSpawnModuleEditorTab(const FSpawnTabArgs& SpawnTabArgs)
{
	const TSharedRef<SDockTab> NomadTab = SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		.Label(NSLOCTEXT("ModuleEditor", "ModuleEditorTabTitle", "Module Editor"));

	SAssignNewEd(ModuleEditorWidget, SModuleEditorWidget, true, NomadTab);

	NomadTab->SetContent(ModuleEditorWidget->TakeWidget());
	
	return NomadTab;
}

//////////////////////////////////////////////////////////////////////////
/// ModuleBlueprintEditor
FModuleBlueprintEditor::FModuleBlueprintEditor()
{
	BlueprintClass = UModuleBlueprint::StaticClass();
	GraphSchemaClass = UModuleGraphSchema::StaticClass();
	
	ToolkitFName = FName("ModuleBlueprintEditor");
	BaseToolkitName = LOCTEXT("ModuleBlueprintEditorAppLabel", "Module Blueprint Editor");
	ToolkitNameFormat = LOCTEXT("ModuleBlueprintEditorToolkitName", "{ObjectName}{DirtyState}");

	WorldCentricTabPrefix = TEXT("ModuleBlueprintEditor");
	WorldCentricTabColorScale = FLinearColor::White;
}

#undef LOCTEXT_NAMESPACE

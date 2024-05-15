// Copyright Epic Games, Inc. All Rights Reserved.

#include "Asset/AssetEditor.h"

#include "IAssetTools.h"
#include "ISettingsSection.h"
#include "WHFrameworkSlateStatics.h"
#include "Asset/AssetEditorTypes.h"
#include "Asset/Slate/SAssetModifierEditorWidget.h"

#define LOCTEXT_NAMESPACE "FAssetModifierEditor"

//////////////////////////////////////////////////////////////////////////
/// AssetEditorModule
IMPLEMENTATION_EDITOR_MODULE(FAssetEditorModule)

static const FName AssetModifierEditorTabName("AssetModifierEditor");

FAssetEditorModule::FAssetEditorModule()
{
	AppIdentifier = FName("AssetEditorApp");
}

void FAssetEditorModule::StartupModule()
{
	FAssetModifierEditorCommands::Register();

	// Register tabs
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(AssetModifierEditorTabName, FOnSpawnTab::CreateRaw(this, &FAssetEditorModule::OnSpawnAssetModifierEditorTab))
		.SetDisplayName(LOCTEXT("FAssetModifierEditorTabTitle", "Asset Modifier Editor"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);
}

void FAssetEditorModule::ShutdownModule()
{
	FAssetModifierEditorCommands::Unregister();
}

void FAssetEditorModule::RegisterSettings(ISettingsModule* SettingsModule)
{
	GAssetEditorIni = GConfig->GetDestIniFilename(TEXT("AssetModifierEditor"), *UGameplayStatics::GetPlatformName(), *FPaths::GeneratedConfigDir());
	const ISettingsSectionPtr SettingsSection = SettingsModule->RegisterSettings(FName("Project"), FName("WHFramework"), FName("Asset Modifier Editor"), FText::FromString(TEXT("Asset Modifier Editor")), FText::FromString(TEXT("Configure the asset modifier editor plugin")), GetMutableDefault<UAssetModifierEditorSettings>());
	if(SettingsSection.IsValid())
	{
		SettingsSection->OnModified().BindRaw(this, &FAssetEditorModule::HandleSettingsSaved);
	}
}

void FAssetEditorModule::UnRegisterSettings(ISettingsModule* SettingsModule)
{
	SettingsModule->UnregisterSettings(FName("Project"), FName("Plugins"), FName("Module Editor"));
}

bool FAssetEditorModule::HandleSettingsSaved()
{
	UAssetModifierEditorSettings* AssetModifierEditorSetting = GetMutableDefault<UAssetModifierEditorSettings>();
	AssetModifierEditorSetting->SaveConfig();
	return true;
}

void FAssetEditorModule::RegisterCommands(const TSharedPtr<FUICommandList>& Commands)
{
	Commands->MapAction(
		FAssetModifierEditorCommands::Get().OpenAssetModifierEditorWindow,
		FExecuteAction::CreateRaw(this, &FAssetEditorModule::OpenAssetModifierEditor),
		FCanExecuteAction());
}

void FAssetEditorModule::RegisterMenus(const TSharedPtr<FUICommandList>& Commands)
{
	AddWindowMenu(FAssetModifierEditorCommands::Get().OpenAssetModifierEditorWindow, Commands);
	
	AddToolbarMenu(FAssetModifierEditorCommands::Get().OpenAssetModifierEditorWindow, Commands);
}

void FAssetEditorModule::RegisterAssetTypeAction(IAssetTools& AssetTools, EAssetTypeCategories::Type AssetCategory, TArray<TSharedPtr<IAssetTypeActions>>& AssetTypeActions)
{
}

void FAssetEditorModule::RegisterCustomization(FPropertyEditorModule& PropertyEditor)
{
	FEditorModuleBase::RegisterCustomization(PropertyEditor);
}

void FAssetEditorModule::UnRegisterCustomization(FPropertyEditorModule& PropertyEditor)
{
}

void FAssetEditorModule::OpenAssetModifierEditor()
{
	FGlobalTabmanager::Get()->TryInvokeTab(FName("AssetModifierEditor"));
}

TSharedRef<SDockTab> FAssetEditorModule::OnSpawnAssetModifierEditorTab(const FSpawnTabArgs& SpawnTabArgs)
{
	const TSharedRef<SDockTab> NomadTab = SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		.Label(NSLOCTEXT("AssetModifierEditor", "AssetModifierEditorTabTitle", "Asset Modifier Editor"));

	SAssignNewEd(AssetModifierEditorWidget, SAssetModifierEditorWidget, true, NomadTab);

	NomadTab->SetContent(AssetModifierEditorWidget->TakeWidget());
	
	return NomadTab;
}

#undef LOCTEXT_NAMESPACE

// Copyright Epic Games, Inc. All Rights Reserved.

#include "WHFrameworkEditor.h"

#include "AssetToolsModule.h"
#include "ISettingsModule.h"
#include "ISettingsSection.h"
#include "LevelEditor.h"
#include "Widgets/Docking/SDockTab.h"
#include "ToolMenus.h"
#include "WHFrameworkEditorCommands.h"
#include "WHFrameworkEditorStyle.h"
#include "Kismet/GameplayStatics.h"
#include "Procedure/AssetTypeActions_ProcedureBlueprint.h"
#include "Procedure/ProcedureEditorSettings.h"
#include "Procedure/ProcedureEditorTypes.h"
#include "Procedure/Widget/SProcedureEditorWidget.h"

static const FName ProcedureEditorTabName("ProcedureEditor");

#define LOCTEXT_NAMESPACE "FWHFrameworkEditorModule"

void FWHFrameworkEditorModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module

	RegisterSettings();

	FWHFrameworkEditorStyle::Initialize();
	FWHFrameworkEditorStyle::ReloadTextures();

	FWHFrameworkEditorCommands::Register();
	
	PluginCommands = MakeShareable(new FUICommandList);
	
	PluginCommands->MapAction(
		FWHFrameworkEditorCommands::Get().OpenProcedureEditorWindow,
		FExecuteAction::CreateRaw(this, &FWHFrameworkEditorModule::OnClickedProcedureEditorButton),
		FCanExecuteAction());
	
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FWHFrameworkEditorModule::RegisterMenus));

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(ProcedureEditorTabName, FOnSpawnTab::CreateRaw(this, &FWHFrameworkEditorModule::OnSpawnProcedureEditorTab))
		.SetDisplayName(LOCTEXT("FProcedureEditorTabTitle", "ProcedureEditor"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);

	// Register asset types
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	EAssetTypeCategories::Type AssetCategory = AssetTools.RegisterAdvancedAssetCategory(FName("WHFramework"), FText::FromString(TEXT("WHFramework")));
	TSharedRef<IAssetTypeActions> PBAction = MakeShareable(new FAssetTypeActions_ProcedureBlueprint(AssetCategory));
	RegisterAssetTypeAction(AssetTools, PBAction);
}

void FWHFrameworkEditorModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	if(UObjectInitialized())
	{
		UnRegisterSettings();
	}
	
	UToolMenus::UnRegisterStartupCallback(this);

	UToolMenus::UnregisterOwner(this);

	FWHFrameworkEditorStyle::Shutdown();

	FWHFrameworkEditorCommands::Unregister();

	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(ProcedureEditorTabName);

	// Unregister asset type actions
	if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
	{
		IAssetTools& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();
		for (auto& AssetTypeAction : CreatedAssetTypeActions)
		{
			if (AssetTypeAction.IsValid())
			{
				AssetToolsModule.UnregisterAssetTypeActions(AssetTypeAction.ToSharedRef());
			}
		}
	}
	CreatedAssetTypeActions.Empty();
}

void FWHFrameworkEditorModule::RegisterMenus()
{
	// Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
	FToolMenuOwnerScoped OwnerScoped(this);

	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
		{
			FToolMenuSection& Section = Menu->FindOrAddSection("WHFramework");
			Section.AddMenuEntryWithCommandList(FWHFrameworkEditorCommands::Get().OpenProcedureEditorWindow, PluginCommands);
		}
	}

	{
		UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar");
		{
			FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("WHFramework");
			{
				FToolMenuEntry& Entry = Section.AddEntry(FToolMenuEntry::InitToolBarButton(FWHFrameworkEditorCommands::Get().OpenProcedureEditorWindow));
				Entry.SetCommandList(PluginCommands);
			}
		}
	}
}

void FWHFrameworkEditorModule::RegisterSettings()
{
	if(ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>(FName("Settings")))
	{
		GProcedureEditorIni = GConfig->GetDestIniFilename(TEXT("ProcedureEditor"), *UGameplayStatics::GetPlatformName(), *FPaths::GeneratedConfigDir());

		// ISettingsContainerPtr SettingsContainer = SettingsModule->GetContainer(FName("Project"));
		// SettingsContainer->DescribeCategory(FName("ProcedureEditor"), FText::FromString(TEXT("ProcedureEditor")), FText::FromString(TEXT("ProcedureEditor")));

		ISettingsSectionPtr SettingsSection = SettingsModule->RegisterSettings(FName("Project"), FName("Plugins"), FName("Procedure Editor"), FText::FromString(TEXT("Procedure Editor")), FText::FromString(TEXT("Configure the procedure editor plugin")), GetMutableDefault<UProcedureEditorSettings>());

		if(SettingsSection.IsValid())
		{
			SettingsSection->OnModified().BindRaw(this, &FWHFrameworkEditorModule::HandleSettingsSaved);
		}
	}
}

void FWHFrameworkEditorModule::UnRegisterSettings()
{
	if(ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>(FName("Settings")))
	{
		SettingsModule->UnregisterSettings(FName("Project"), FName("Plugins"), FName("Procedure Editor"));
	}
}

bool FWHFrameworkEditorModule::HandleSettingsSaved()
{
	UProcedureEditorSettings* ProcedureEditorSetting = GetMutableDefault<UProcedureEditorSettings>();
	
	ProcedureEditorSetting->SaveConfig();

	return true;
}

void FWHFrameworkEditorModule::RegisterAssetTypeAction(class IAssetTools& AssetTools, TSharedRef<IAssetTypeActions> Action)
{
	AssetTools.RegisterAssetTypeActions(Action);
	CreatedAssetTypeActions.Add(Action);
}

void FWHFrameworkEditorModule::OnClickedProcedureEditorButton()
{
	FGlobalTabmanager::Get()->TryInvokeTab(ProcedureEditorTabName);
}

TSharedRef<SDockTab> FWHFrameworkEditorModule::OnSpawnProcedureEditorTab(const FSpawnTabArgs& SpawnTabArgs)
{
	return SNew(SDockTab).TabRole(ETabRole::NomadTab)
	[
		SAssignNew(ProcedureEditorWidget, SProcedureEditorWidget)
	];
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FWHFrameworkEditorModule, WHFrameworkEditor)
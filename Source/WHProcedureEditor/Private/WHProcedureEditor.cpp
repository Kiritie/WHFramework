// Copyright Epic Games, Inc. All Rights Reserved.

#include "WHProcedureEditor.h"

#include "ISettingsModule.h"
#include "ISettingsSection.h"
#include "WHProcedureEditorStyle.h"
#include "WHProcedureEditorCommands.h"
#include "WHProcedureEditorSettings.h"
#include "LevelEditor.h"
#include "Widgets/Docking/SDockTab.h"
#include "ToolMenus.h"
#include "Kismet/GameplayStatics.h"
#include "Widget/SProcedureMainWidget.h"

static const FName WHProcedureEditorTabName("WHProcedureEditor");

#define LOCTEXT_NAMESPACE "FWHProcedureEditorModule"

void FWHProcedureEditorModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module

	RegisterSettings();

	FWHProcedureEditorStyle::Initialize();
	FWHProcedureEditorStyle::ReloadTextures();

	FWHProcedureEditorCommands::Register();
	
	PluginCommands = MakeShareable(new FUICommandList);
	
	PluginCommands->MapAction(
		FWHProcedureEditorCommands::Get().OpenPluginWindow,
		FExecuteAction::CreateRaw(this, &FWHProcedureEditorModule::PluginButtonClicked),
		FCanExecuteAction());
	
	// UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FWHProcedureEditorModule::RegisterMenus));

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(WHProcedureEditorTabName, FOnSpawnTab::CreateRaw(this, &FWHProcedureEditorModule::OnSpawnPluginTab))
		.SetDisplayName(LOCTEXT("FWHProcedureEditorTabTitle", "WHProcedureEditor"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);
}

void FWHProcedureEditorModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	if(UObjectInitialized())
	{
		UnRegisterSettings();
	}
	
	UToolMenus::UnRegisterStartupCallback(this);

	UToolMenus::UnregisterOwner(this);

	FWHProcedureEditorStyle::Shutdown();

	FWHProcedureEditorCommands::Unregister();

	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(WHProcedureEditorTabName);
}

TSharedRef<SDockTab> FWHProcedureEditorModule::OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs)
{
	return SNew(SDockTab).TabRole(ETabRole::NomadTab)
	[
		SAssignNew(ProcedureMainWidget, SProcedureMainWidget)
	];
}

void FWHProcedureEditorModule::PluginButtonClicked()
{
	FGlobalTabmanager::Get()->TryInvokeTab(WHProcedureEditorTabName);
}

void FWHProcedureEditorModule::RegisterMenus()
{
	// Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
	FToolMenuOwnerScoped OwnerScoped(this);

	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
		{
			FToolMenuSection& Section = Menu->FindOrAddSection("WindowLayout");
			Section.AddMenuEntryWithCommandList(FWHProcedureEditorCommands::Get().OpenPluginWindow, PluginCommands);
		}
	}

	{
		UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar");
		{
			FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("Settings");
			{
				FToolMenuEntry& Entry = Section.AddEntry(FToolMenuEntry::InitToolBarButton(FWHProcedureEditorCommands::Get().OpenPluginWindow));
				Entry.SetCommandList(PluginCommands);
			}
		}
	}
}

void FWHProcedureEditorModule::RegisterSettings()
{
	if(ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>(FName("Settings")))
	{
		GProcedureEditorIni = GConfig->GetDestIniFilename(TEXT("ProcedureEditor"), *UGameplayStatics::GetPlatformName(), *FPaths::GeneratedConfigDir());

		// ISettingsContainerPtr SettingsContainer = SettingsModule->GetContainer(FName("Project"));
		// SettingsContainer->DescribeCategory(FName("ProcedureEditor"), FText::FromString(TEXT("ProcedureEditor")), FText::FromString(TEXT("ProcedureEditor")));

		ISettingsSectionPtr SettingsSection = SettingsModule->RegisterSettings(FName("Project"), FName("Plugins"), FName("Procedure Editor"), FText::FromString(TEXT("Procedure Editor")), FText::FromString(TEXT("Configure the procedure editor plugin")), GetMutableDefault<UWHProcedureEditorSettings>());

		if(SettingsSection.IsValid())
		{
			SettingsSection->OnModified().BindRaw(this, &FWHProcedureEditorModule::HandleSettingsSaved);
		}
	}
}

void FWHProcedureEditorModule::UnRegisterSettings()
{
	if(ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>(FName("Settings")))
	{
		SettingsModule->UnregisterSettings(FName("Project"), FName("Plugins"), FName("Procedure Editor"));
	}
}

bool FWHProcedureEditorModule::HandleSettingsSaved()
{
	UWHProcedureEditorSettings* ProcedureEditorSetting = GetMutableDefault<UWHProcedureEditorSettings>();
	
	ProcedureEditorSetting->SaveConfig();

	return true;
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FWHProcedureEditorModule, WHProcedureEditor)
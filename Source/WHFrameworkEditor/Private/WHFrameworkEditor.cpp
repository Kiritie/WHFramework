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
#include "Camera/CameraModuleDetailsPanel.h"
#include "Event/EventModuleBPLibrary.h"
#include "Event/Handle/Global/EventHandle_BeginPlay.h"
#include "Event/Handle/Global/EventHandle_EndPlay.h"
#include "FSM/AssetTypeActions_FiniteStateBlueprint.h"
#include "Global/GlobalTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Procedure/AssetTypeActions_ProcedureBlueprint.h"
#include "Procedure/ProcedureDetailsPanel.h"
#include "Procedure/ProcedureEditorSettings.h"
#include "Procedure/ProcedureEditorTypes.h"
#include "Procedure/ProcedureModuleDetailsPanel.h"
#include "Procedure/Widget/SProcedureEditorWidget.h"
#include "Step/AssetTypeActions_StepBlueprint.h"
#include "Step/StepDetailsPanel.h"
#include "Step/StepEditorSettings.h"
#include "Step/StepEditorTypes.h"
#include "Step/StepModuleDetailsPanel.h"
#include "Step/Widget/SStepEditorWidget.h"

static const FName ProcedureEditorTabName("ProcedureEditor");

static const FName StepEditorTabName("StepEditor");

#define LOCTEXT_NAMESPACE "FWHFrameworkEditorModule"

void FWHFrameworkEditorModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	
	// Register delegates
	if(BeginPIEDelegateHandle.IsValid())
	{
		FEditorDelegates::PostPIEStarted.Remove(BeginPIEDelegateHandle);
	}
	BeginPIEDelegateHandle = FEditorDelegates::PostPIEStarted.AddRaw(this, &FWHFrameworkEditorModule::OnBeginPIE);

	if(EndPIEDelegateHandle.IsValid())
	{
		FEditorDelegates::EndPIE.Remove(EndPIEDelegateHandle);
	}
	EndPIEDelegateHandle = FEditorDelegates::EndPIE.AddRaw(this, &FWHFrameworkEditorModule::OnEndPIE);

	// Register settings
	RegisterSettings();

	// Register styles
	FWHFrameworkEditorStyle::Initialize();
	FWHFrameworkEditorStyle::ReloadTextures();

	// Register commands
	FWHFrameworkEditorCommands::Register();

	PluginCommands = MakeShareable(new FUICommandList);

	PluginCommands->MapAction(
		FWHFrameworkEditorCommands::Get().OpenProcedureEditorWindow,
		FExecuteAction::CreateRaw(this, &FWHFrameworkEditorModule::OnClickedProcedureEditorButton),
		FCanExecuteAction());

	PluginCommands->MapAction(
		FWHFrameworkEditorCommands::Get().OpenStepEditorWindow,
		FExecuteAction::CreateRaw(this, &FWHFrameworkEditorModule::OnClickedStepEditorButton),
		FCanExecuteAction());

	// Register menus
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FWHFrameworkEditorModule::RegisterMenus));

	// Register tabs
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(ProcedureEditorTabName, FOnSpawnTab::CreateRaw(this, &FWHFrameworkEditorModule::OnSpawnProcedureEditorTab))
		.SetDisplayName(LOCTEXT("FProcedureEditorTabTitle", "Procedure Editor"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(StepEditorTabName, FOnSpawnTab::CreateRaw(this, &FWHFrameworkEditorModule::OnSpawnStepEditorTab))
	.SetDisplayName(LOCTEXT("FStepEditorTabTitle", "Step Editor"))
	.SetMenuType(ETabSpawnerMenuType::Hidden);

	// Register asset types
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	EAssetTypeCategories::Type AssetCategory = AssetTools.RegisterAdvancedAssetCategory(FName("WHFramework"), FText::FromString(TEXT("WHFramework")));

	TSharedRef<IAssetTypeActions> PBAction = MakeShareable(new FAssetTypeActions_ProcedureBlueprint(AssetCategory));
	RegisterAssetTypeAction(AssetTools, PBAction);

	TSharedRef<IAssetTypeActions> SBAction = MakeShareable(new FAssetTypeActions_StepBlueprint(AssetCategory));
	RegisterAssetTypeAction(AssetTools, SBAction);

	TSharedRef<IAssetTypeActions> FSAction = MakeShareable(new FAssetTypeActions_FiniteStateBlueprint(AssetCategory));
	RegisterAssetTypeAction(AssetTools, FSAction);

	// Register the details customizer
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));

	PropertyModule.RegisterCustomClassLayout(TEXT("CameraModule"), FOnGetDetailCustomizationInstance::CreateStatic(&FCameraModuleDetailsPanel::MakeInstance));

	PropertyModule.RegisterCustomClassLayout(TEXT("ProcedureBase"), FOnGetDetailCustomizationInstance::CreateStatic(&FProcedureDetailsPanel::MakeInstance));
	PropertyModule.RegisterCustomClassLayout(TEXT("ProcedureModule"), FOnGetDetailCustomizationInstance::CreateStatic(&FProcedureModuleDetailsPanel::MakeInstance));

	PropertyModule.RegisterCustomClassLayout(TEXT("StepBase"), FOnGetDetailCustomizationInstance::CreateStatic(&FStepDetailsPanel::MakeInstance));
	PropertyModule.RegisterCustomClassLayout(TEXT("StepModule"), FOnGetDetailCustomizationInstance::CreateStatic(&FStepModuleDetailsPanel::MakeInstance));
}

void FWHFrameworkEditorModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	if(UObjectInitialized())
	{
		UnRegisterSettings();
	}

	if(BeginPIEDelegateHandle.IsValid())
	{
		FEditorDelegates::PostPIEStarted.Remove(BeginPIEDelegateHandle);
	}

	if(EndPIEDelegateHandle.IsValid())
	{
		FEditorDelegates::EndPIE.Remove(EndPIEDelegateHandle);
	}

	UToolMenus::UnRegisterStartupCallback(this);

	UToolMenus::UnregisterOwner(this);

	FWHFrameworkEditorStyle::Shutdown();

	FWHFrameworkEditorCommands::Unregister();

	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(ProcedureEditorTabName);

	// Unregister asset type actions
	if(FModuleManager::Get().IsModuleLoaded(TEXT("AssetTools")))
	{
		IAssetTools& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
		for(auto& AssetTypeAction : CreatedAssetTypeActions)
		{
			if(AssetTypeAction.IsValid())
			{
				AssetToolsModule.UnregisterAssetTypeActions(AssetTypeAction.ToSharedRef());
			}
		}
	}

	// Unregister the details customization
	if(FModuleManager::Get().IsModuleLoaded(TEXT("PropertyEditor")))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));

		PropertyModule.UnregisterCustomClassLayout(TEXT("CameraModule"));

		PropertyModule.UnregisterCustomClassLayout(TEXT("ProcedureBase"));
		PropertyModule.UnregisterCustomClassLayout(TEXT("ProcedureModule"));
	
		PropertyModule.UnregisterCustomClassLayout(TEXT("StepBase"));
		PropertyModule.UnregisterCustomClassLayout(TEXT("StepModule"));

		PropertyModule.NotifyCustomizationModuleChanged();
	}

	CreatedAssetTypeActions.Empty();
}

void FWHFrameworkEditorModule::RegisterMenus()
{
	// Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
	FToolMenuOwnerScoped OwnerScoped(this);

	UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
	{
		FToolMenuSection& Section = Menu->FindOrAddSection("WHFramework");
		Section.AddMenuEntryWithCommandList(FWHFrameworkEditorCommands::Get().OpenProcedureEditorWindow, PluginCommands);
		Section.AddMenuEntryWithCommandList(FWHFrameworkEditorCommands::Get().OpenStepEditorWindow, PluginCommands);
	}

	UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar");
	{
		FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("WHFramework");
		{
			FToolMenuEntry& Entry1 = Section.AddEntry(FToolMenuEntry::InitToolBarButton(FWHFrameworkEditorCommands::Get().OpenProcedureEditorWindow));
			Entry1.SetCommandList(PluginCommands);

			FToolMenuEntry& Entry2 = Section.AddEntry(FToolMenuEntry::InitToolBarButton(FWHFrameworkEditorCommands::Get().OpenStepEditorWindow));
			Entry2.SetCommandList(PluginCommands);
		}
	}
}

void FWHFrameworkEditorModule::RegisterSettings()
{
	if(ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>(FName("Settings")))
	{
		GProcedureEditorIni = GConfig->GetDestIniFilename(TEXT("ProcedureEditor"), *UGameplayStatics::GetPlatformName(), *FPaths::GeneratedConfigDir());

		ISettingsSectionPtr SettingsSection1 = SettingsModule->RegisterSettings(FName("Project"), FName("Plugins"), FName("Procedure Editor"), FText::FromString(TEXT("Procedure Editor")), FText::FromString(TEXT("Configure the procedure editor plugin")), GetMutableDefault<UProcedureEditorSettings>());

		if(SettingsSection1.IsValid())
		{
			SettingsSection1->OnModified().BindRaw(this, &FWHFrameworkEditorModule::HandleSettingsSaved);
		}
		
		///
		GStepEditorIni = GConfig->GetDestIniFilename(TEXT("StepEditor"), *UGameplayStatics::GetPlatformName(), *FPaths::GeneratedConfigDir());

		ISettingsSectionPtr SettingsSection2 = SettingsModule->RegisterSettings(FName("Project"), FName("Plugins"), FName("Step Editor"), FText::FromString(TEXT("Step Editor")), FText::FromString(TEXT("Configure the Step editor plugin")), GetMutableDefault<UStepEditorSettings>());

		if(SettingsSection2.IsValid())
		{
			SettingsSection2->OnModified().BindRaw(this, &FWHFrameworkEditorModule::HandleSettingsSaved);
		}
	}
}

void FWHFrameworkEditorModule::UnRegisterSettings()
{
	if(ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>(FName("Settings")))
	{
		SettingsModule->UnregisterSettings(FName("Project"), FName("Plugins"), FName("Procedure Editor"));
		SettingsModule->UnregisterSettings(FName("Project"), FName("Plugins"), FName("Step Editor"));
	}
}

bool FWHFrameworkEditorModule::HandleSettingsSaved()
{
	UProcedureEditorSettings* ProcedureEditorSetting = GetMutableDefault<UProcedureEditorSettings>();

	ProcedureEditorSetting->SaveConfig();

	UStepEditorSettings* StepEditorSetting = GetMutableDefault<UStepEditorSettings>();

	StepEditorSetting->SaveConfig();

	return true;
}

void FWHFrameworkEditorModule::RegisterAssetTypeAction(class IAssetTools& AssetTools, TSharedRef<IAssetTypeActions> Action)
{
	AssetTools.RegisterAssetTypeActions(Action);
	CreatedAssetTypeActions.Add(Action);
}

void FWHFrameworkEditorModule::OnBeginPIE(bool bIsSimulating)
{
	UEventModuleBPLibrary::BroadcastEvent(UEventHandle_BeginPlay::StaticClass(), EEventNetType::Single, nullptr, { FParameter::MakeBoolean(bIsSimulating) });
}

void FWHFrameworkEditorModule::OnEndPIE(bool bIsSimulating)
{
	UEventModuleBPLibrary::BroadcastEvent(UEventHandle_EndPlay::StaticClass(), EEventNetType::Single, nullptr, { FParameter::MakeBoolean(bIsSimulating) });
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

void FWHFrameworkEditorModule::OnClickedStepEditorButton()
{
	FGlobalTabmanager::Get()->TryInvokeTab(StepEditorTabName);
}

TSharedRef<SDockTab> FWHFrameworkEditorModule::OnSpawnStepEditorTab(const FSpawnTabArgs& SpawnTabArgs)
{
	return SNew(SDockTab).TabRole(ETabRole::NomadTab)
	[
		SAssignNew(StepEditorWidget, SStepEditorWidget)
	];
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FWHFrameworkEditorModule, WHFrameworkEditor)

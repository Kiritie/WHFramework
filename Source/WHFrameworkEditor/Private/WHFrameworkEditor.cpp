// Copyright Epic Games, Inc. All Rights Reserved.

#include "WHFrameworkEditor.h"

#include "AssetToolsModule.h"
#include "ISettingsModule.h"
#include "ISettingsSection.h"
#include "Widgets/Docking/SDockTab.h"
#include "ToolMenus.h"
#include "WHFrameworkEditorCommands.h"
#include "WHFrameworkEditorStyle.h"
#include "Achievement/AchievementEditor.h"
#include "Camera/CameraModuleDetailsPanel.h"
#include "FSM/FiniteStateBlueprintActions.h"
#include "FSM/FSMComponentDetailsPanel.h"
#include "Kismet/GameplayStatics.h"
#include "Main/MainModuleDetailsPanel.h"
#include "Main/ModuleBlueprintActions.h"
#include "Main/ModuleEditor.h"
#include "Main/ModuleEditorSettings.h"
#include "Procedure/ProcedureBlueprintActions.h"
#include "Procedure/ProcedureDetailsPanel.h"
#include "Procedure/ProcedureEditor.h"
#include "Procedure/ProcedureEditorSettings.h"
#include "Procedure/ProcedureEditorTypes.h"
#include "Procedure/ProcedureModuleDetailsPanel.h"
#include "Step/StepBlueprintActions.h"
#include "Step/StepDetailsPanel.h"
#include "Step/StepEditor.h"
#include "Step/StepEditorSettings.h"
#include "Step/StepEditorTypes.h"
#include "Step/StepModuleDetailsPanel.h"
#include "Task/TaskBlueprintActions.h"
#include "Task/TaskDetailsPanel.h"
#include "Task/TaskEditor.h"
#include "Task/TaskEditorSettings.h"
#include "Task/TaskEditorTypes.h"
#include "Task/TaskModuleDetailsPanel.h"

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

	FTaskEditorCommands::Register();

	FModuleEditor::Get().StartupModule();
	FAchievementEditor::Get().StartupModule();
	FProcedureEditor::Get().StartupModule();
	FStepEditor::Get().StartupModule();
	FTaskEditor::Get().StartupModule();

	PluginCommands = MakeShareable(new FUICommandList);

	FModuleEditor::Get().RegisterCommands(PluginCommands);
	FAchievementEditor::Get().RegisterCommands(PluginCommands);
	FProcedureEditor::Get().RegisterCommands(PluginCommands);
	FStepEditor::Get().RegisterCommands(PluginCommands);
	FTaskEditor::Get().RegisterCommands(PluginCommands);

	// Register menus
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FWHFrameworkEditorModule::RegisterMenus));

	// Register asset types
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	EAssetTypeCategories::Type AssetCategory = AssetTools.RegisterAdvancedAssetCategory(FName("WHFramework"), FText::FromString(TEXT("WHFramework")));

	TSharedRef<IAssetTypeActions> MBAction = MakeShareable(new FModuleBlueprintActions(AssetCategory));
	RegisterAssetTypeAction(AssetTools, MBAction);

	TSharedRef<IAssetTypeActions> PBAction = MakeShareable(new FProcedureBlueprintActions(AssetCategory));
	RegisterAssetTypeAction(AssetTools, PBAction);

	TSharedRef<IAssetTypeActions> SBAction = MakeShareable(new FStepBlueprintActions(AssetCategory));
	RegisterAssetTypeAction(AssetTools, SBAction);

	TSharedRef<IAssetTypeActions> TBAction = MakeShareable(new FTaskBlueprintActions(AssetCategory));
	RegisterAssetTypeAction(AssetTools, TBAction);

	TSharedRef<IAssetTypeActions> FSAction = MakeShareable(new FFiniteStateBlueprintActions(AssetCategory));
	RegisterAssetTypeAction(AssetTools, FSAction);

	// Register the details customizer
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));

	PropertyModule.RegisterCustomClassLayout(TEXT("MainModule"), FOnGetDetailCustomizationInstance::CreateStatic(&FMainModuleDetailsPanel::MakeInstance));

	PropertyModule.RegisterCustomClassLayout(TEXT("CameraModule"), FOnGetDetailCustomizationInstance::CreateStatic(&FCameraModuleDetailsPanel::MakeInstance));

	PropertyModule.RegisterCustomClassLayout(TEXT("FSMComponent"), FOnGetDetailCustomizationInstance::CreateStatic(&FFSMComponentDetailsPanel::MakeInstance));

	PropertyModule.RegisterCustomClassLayout(TEXT("ProcedureBase"), FOnGetDetailCustomizationInstance::CreateStatic(&FProcedureDetailsPanel::MakeInstance));
	PropertyModule.RegisterCustomClassLayout(TEXT("ProcedureModule"), FOnGetDetailCustomizationInstance::CreateStatic(&FProcedureModuleDetailsPanel::MakeInstance));

	PropertyModule.RegisterCustomClassLayout(TEXT("StepBase"), FOnGetDetailCustomizationInstance::CreateStatic(&FStepDetailsPanel::MakeInstance));
	PropertyModule.RegisterCustomClassLayout(TEXT("StepModule"), FOnGetDetailCustomizationInstance::CreateStatic(&FStepModuleDetailsPanel::MakeInstance));

	PropertyModule.RegisterCustomClassLayout(TEXT("TaskBase"), FOnGetDetailCustomizationInstance::CreateStatic(&FTaskDetailsPanel::MakeInstance));
	PropertyModule.RegisterCustomClassLayout(TEXT("TaskModule"), FOnGetDetailCustomizationInstance::CreateStatic(&FTaskModuleDetailsPanel::MakeInstance));
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

	FModuleEditor::Get().ShutdownModule();
	FAchievementEditor::Get().ShutdownModule();
	FProcedureEditor::Get().ShutdownModule();
	FStepEditor::Get().ShutdownModule();
	FTaskEditor::Get().ShutdownModule();

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

		PropertyModule.UnregisterCustomClassLayout(TEXT("MainModule"));
		
		PropertyModule.UnregisterCustomClassLayout(TEXT("CameraModule"));
		
		PropertyModule.UnregisterCustomClassLayout(TEXT("FSMComponent"));

		PropertyModule.UnregisterCustomClassLayout(TEXT("ProcedureBase"));
		PropertyModule.UnregisterCustomClassLayout(TEXT("ProcedureModule"));
	
		PropertyModule.UnregisterCustomClassLayout(TEXT("StepBase"));
		PropertyModule.UnregisterCustomClassLayout(TEXT("StepModule"));
	
		PropertyModule.UnregisterCustomClassLayout(TEXT("TaskBase"));
		PropertyModule.UnregisterCustomClassLayout(TEXT("TaskModule"));

		PropertyModule.NotifyCustomizationModuleChanged();
	}

	CreatedAssetTypeActions.Empty();
}

void FWHFrameworkEditorModule::RegisterMenus()
{
	// Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
	FToolMenuOwnerScoped OwnerScoped(this);

	UToolMenu* WindowMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
	{
		FToolMenuSection& Section1 = WindowMenu->FindOrAddSection("Level Editor");
		{
			Section1.AddSubMenu(
				"WHFramework",
				LOCTEXT("WHFramework", "WHFramework"),
				LOCTEXT("WHFramework_ToolTip", "WHFramework"),
				FNewToolMenuDelegate::CreateLambda([this](UToolMenu* InNewToolMenu)
				{
					FToolMenuSection& Section2 = InNewToolMenu->FindOrAddSection("WHFramework");
					Section2.AddMenuEntryWithCommandList(FWHFrameworkEditorCommands::Get().OpenModuleEditorWindow, PluginCommands);
					Section2.AddMenuEntryWithCommandList(FWHFrameworkEditorCommands::Get().OpenProcedureEditorWindow, PluginCommands);
					Section2.AddMenuEntryWithCommandList(FWHFrameworkEditorCommands::Get().OpenStepEditorWindow, PluginCommands);
					Section2.AddMenuEntryWithCommandList(FWHFrameworkEditorCommands::Get().OpenTaskEditorWindow, PluginCommands);
				}),
				false,
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "Persona.AssetActions.CreateAnimAsset")
			);
		}
	}

	UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.PlayToolBar");
	{
		FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("WHFramework");
		{
			FToolMenuEntry& Entry1 = Section.AddEntry(FToolMenuEntry::InitToolBarButton(FWHFrameworkEditorCommands::Get().OpenModuleEditorWindow));
			Entry1.SetCommandList(PluginCommands);

			FToolMenuEntry& Entry2 = Section.AddEntry(FToolMenuEntry::InitToolBarButton(FWHFrameworkEditorCommands::Get().OpenProcedureEditorWindow));
			Entry2.SetCommandList(PluginCommands);

			FToolMenuEntry& Entry3 = Section.AddEntry(FToolMenuEntry::InitToolBarButton(FWHFrameworkEditorCommands::Get().OpenStepEditorWindow));
			Entry3.SetCommandList(PluginCommands);

			FToolMenuEntry& Entry4 = Section.AddEntry(FToolMenuEntry::InitToolBarButton(FWHFrameworkEditorCommands::Get().OpenTaskEditorWindow));
			Entry4.SetCommandList(PluginCommands);
		}
	}
}

void FWHFrameworkEditorModule::RegisterSettings()
{
	if(ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>(FName("Settings")))
	{
		GModuleEditorIni = GConfig->GetDestIniFilename(TEXT("ModuleEditor"), *UGameplayStatics::GetPlatformName(), *FPaths::GeneratedConfigDir());
		ISettingsSectionPtr SettingsSection = SettingsModule->RegisterSettings(FName("Project"), FName("WHFramework"), FName("Module Editor"), FText::FromString(TEXT("Module Editor")), FText::FromString(TEXT("Configure the module editor plugin")), GetMutableDefault<UModuleEditorSettings>());
		if(SettingsSection.IsValid())
		{
			SettingsSection->OnModified().BindRaw(this, &FWHFrameworkEditorModule::HandleSettingsSaved);
		}
		
		///-----------------
		GProcedureEditorIni = GConfig->GetDestIniFilename(TEXT("ProcedureEditor"), *UGameplayStatics::GetPlatformName(), *FPaths::GeneratedConfigDir());
		SettingsSection = SettingsModule->RegisterSettings(FName("Project"), FName("WHFramework"), FName("Procedure Editor"), FText::FromString(TEXT("Procedure Editor")), FText::FromString(TEXT("Configure the procedure editor plugin")), GetMutableDefault<UProcedureEditorSettings>());
		if(SettingsSection.IsValid())
		{
			SettingsSection->OnModified().BindRaw(this, &FWHFrameworkEditorModule::HandleSettingsSaved);
		}
		
		///-----------------
		GStepEditorIni = GConfig->GetDestIniFilename(TEXT("StepEditor"), *UGameplayStatics::GetPlatformName(), *FPaths::GeneratedConfigDir());
		SettingsSection = SettingsModule->RegisterSettings(FName("Project"), FName("WHFramework"), FName("Step Editor"), FText::FromString(TEXT("Step Editor")), FText::FromString(TEXT("Configure the Step editor plugin")), GetMutableDefault<UStepEditorSettings>());
		if(SettingsSection.IsValid())
		{
			SettingsSection->OnModified().BindRaw(this, &FWHFrameworkEditorModule::HandleSettingsSaved);
		}
		
		///-----------------
		GTaskEditorIni = GConfig->GetDestIniFilename(TEXT("TaskEditor"), *UGameplayStatics::GetPlatformName(), *FPaths::GeneratedConfigDir());
		SettingsSection = SettingsModule->RegisterSettings(FName("Project"), FName("WHFramework"), FName("Task Editor"), FText::FromString(TEXT("Task Editor")), FText::FromString(TEXT("Configure the Task editor plugin")), GetMutableDefault<UTaskEditorSettings>());
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
		SettingsModule->UnregisterSettings(FName("Project"), FName("Plugins"), FName("Module Editor"));
		SettingsModule->UnregisterSettings(FName("Project"), FName("Plugins"), FName("Procedure Editor"));
		SettingsModule->UnregisterSettings(FName("Project"), FName("Plugins"), FName("Step Editor"));
		SettingsModule->UnregisterSettings(FName("Project"), FName("Plugins"), FName("Task Editor"));
	}
}

bool FWHFrameworkEditorModule::HandleSettingsSaved()
{
	UModuleEditorSettings* ModuleEditorSetting = GetMutableDefault<UModuleEditorSettings>();
	ModuleEditorSetting->SaveConfig();

	UProcedureEditorSettings* ProcedureEditorSetting = GetMutableDefault<UProcedureEditorSettings>();
	ProcedureEditorSetting->SaveConfig();

	UStepEditorSettings* StepEditorSetting = GetMutableDefault<UStepEditorSettings>();
	StepEditorSetting->SaveConfig();

	UTaskEditorSettings* TaskEditorSetting = GetMutableDefault<UTaskEditorSettings>();
	TaskEditorSetting->SaveConfig();

	return true;
}

void FWHFrameworkEditorModule::RegisterAssetTypeAction(class IAssetTools& AssetTools, TSharedRef<IAssetTypeActions> Action)
{
	AssetTools.RegisterAssetTypeActions(Action);
	CreatedAssetTypeActions.Add(Action);
}

void FWHFrameworkEditorModule::OnBeginPIE(bool bIsSimulating)
{
	GIsPlaying = true;
	GIsSimulating = bIsSimulating;
}

void FWHFrameworkEditorModule::OnEndPIE(bool bIsSimulating)
{
	GIsPlaying = false;
	GIsSimulating = false;
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FWHFrameworkEditorModule, WHFrameworkEditor)

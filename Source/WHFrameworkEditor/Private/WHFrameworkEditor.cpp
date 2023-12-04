// Copyright Epic Games, Inc. All Rights Reserved.

#include "WHFrameworkEditor.h"

#include "AssetToolsModule.h"
#include "ISettingsModule.h"
#include "Widgets/Docking/SDockTab.h"
#include "ToolMenus.h"
#include "UnrealEdGlobals.h"
#include "WHFrameworkEditorStyle.h"
#include "WHFrameworkEditorTypes.h"
#include "Achievement/AchievementEditor.h"
#include "Camera/CameraEditor.h"
#include "Character/Base/CharacterBase.h"
#include "Editor/UnrealEdEngine.h"
#include "FSM/FSMEditor.h"
#include "Gameplay/WHGameMode.h"
#include "Main/MainEditorTypes.h"
#include "Pawn/Base/PawnBase.h"
#include "Preferences/UnrealEdOptions.h"
#include "Procedure/ProcedureEditor.h"
#include "Step/StepEditor.h"
#include "Task/TaskEditor.h"
#include "Widget/WidgetEditor.h"

#define LOCTEXT_NAMESPACE "FWHFrameworkEditorModule"

void FWHFrameworkEditorModule::StartupModule()
{
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

	FCoreDelegates::OnPostEngineInit.AddRaw(this, &FWHFrameworkEditorModule::OnPostEngineInit);

	// Register styles
	FWHFrameworkEditorStyle::Initialize();
	FWHFrameworkEditorStyle::ReloadTextures();

	// Register commands
	FWHFrameworkEditorCommands::Register();

	// Startup modules
	StartupEditorModules();

	// Register menus
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FWHFrameworkEditorModule::RegisterMenus));
}

void FWHFrameworkEditorModule::ShutdownModule()
{
	if(BeginPIEDelegateHandle.IsValid())
	{
		FEditorDelegates::PostPIEStarted.Remove(BeginPIEDelegateHandle);
	}

	if(EndPIEDelegateHandle.IsValid())
	{
		FEditorDelegates::EndPIE.Remove(EndPIEDelegateHandle);
	}

	UnRegisterMenus();

	FWHFrameworkEditorStyle::Shutdown();

	FWHFrameworkEditorCommands::Unregister();

	ShutdownEditorModules();
}

void FWHFrameworkEditorModule::StartupEditorModules()
{
	PluginCommands = MakeShareable(new FUICommandList);

	ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>(FName("Settings"));

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	const EAssetTypeCategories::Type AssetCategory = AssetTools.RegisterAdvancedAssetCategory(FName("WHFramework"), FText::FromString(TEXT("WHFramework")));

	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));

	#define STARTUP_MODULE(ModuleName) \
	{ \
		ModuleName::Get().StartupModule();\
		ModuleName::Get().RegisterSettings(SettingsModule); \
		ModuleName::Get().RegisterCommands(PluginCommands);\
		ModuleName::Get().RegisterAssetTypeAction(AssetTools, AssetCategory, CreatedAssetTypeActions);\
		ModuleName::Get().RegisterCustomClassLayout(PropertyModule);\
	}

	STARTUP_MODULE(FAchievementEditorModule);
	STARTUP_MODULE(FCameraEditorModule);
	STARTUP_MODULE(FFSMEditorModule);
	STARTUP_MODULE(FMainEditorModule);
	STARTUP_MODULE(FProcedureEditorModule);
	STARTUP_MODULE(FStepEditorModule);
	STARTUP_MODULE(FTaskEditorModule);
	STARTUP_MODULE(FWidgetEditorModule);
}

void FWHFrameworkEditorModule::ShutdownEditorModules()
{
	ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>(FName("Settings"));

	IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();

	FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));

	#define SHUTDOWN_MODULE(ModuleName) \
	{ \
		ModuleName::Get().ShutdownModule();\
		ModuleName::Get().UnRegisterSettings(SettingsModule); \
		ModuleName::Get().UnRegisterCustomClassLayout(PropertyModule);\
	}

	SHUTDOWN_MODULE(FAchievementEditorModule);
	SHUTDOWN_MODULE(FCameraEditorModule);
	SHUTDOWN_MODULE(FFSMEditorModule);
	SHUTDOWN_MODULE(FMainEditorModule);
	SHUTDOWN_MODULE(FProcedureEditorModule);
	SHUTDOWN_MODULE(FStepEditorModule);
	SHUTDOWN_MODULE(FTaskEditorModule);
	SHUTDOWN_MODULE(FWidgetEditorModule);

	for(auto& AssetTypeAction : CreatedAssetTypeActions)
	{
		if(AssetTypeAction.IsValid())
		{
			AssetTools.UnregisterAssetTypeActions(AssetTypeAction.ToSharedRef());
		}
	}
	CreatedAssetTypeActions.Empty();

	PropertyModule.NotifyCustomizationModuleChanged();
}

void FWHFrameworkEditorModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);

	UToolMenu* WindowMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
	{
		FToolMenuSection& Section1 = WindowMenu->FindOrAddSection("Level Editor");
		{
			Section1.AddSubMenu(
				"WHFramework",
				LOCTEXT("WHFramework", "WHFramework"),
				LOCTEXT("WHFramework_ToolTip", "WHFramework"),
				FNewToolMenuDelegate(),
				false,
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "Persona.AssetActions.CreateAnimAsset")
			);
		}
	}

	FAchievementEditorModule::Get().RegisterMenus(PluginCommands);
	FCameraEditorModule::Get().RegisterMenus(PluginCommands);
	FFSMEditorModule::Get().RegisterMenus(PluginCommands);
	FMainEditorModule::Get().RegisterMenus(PluginCommands);
	FProcedureEditorModule::Get().RegisterMenus(PluginCommands);
	FStepEditorModule::Get().RegisterMenus(PluginCommands);
	FTaskEditorModule::Get().RegisterMenus(PluginCommands);
	FWidgetEditorModule::Get().RegisterMenus(PluginCommands);
}

void FWHFrameworkEditorModule::UnRegisterMenus()
{
	UToolMenus::UnRegisterStartupCallback(this);

	UToolMenus::UnregisterOwner(this);
}

void FWHFrameworkEditorModule::OnPostEngineInit()
{
	if(GUnrealEd)
	{
		GUnrealEd->GetUnrealEdOptions()->OnGetNewAssetDefaultClasses().BindRaw(this, &FWHFrameworkEditorModule::GetNewAssetDefaultClasses);
	}
}

const TArray<FClassPickerDefaults>& FWHFrameworkEditorModule::GetNewAssetDefaultClasses()
{
	if(DefaultClassPickers.IsEmpty())
	{
		auto ClassPickerFunc = [this](const UClass* Class)
		{
			const FSoftClassPath ClassPath(Class);
			const FSoftClassPath AssetClassPath(UBlueprint::StaticClass());
			DefaultClassPickers.Add(FClassPickerDefaults(ClassPath.ToString(), AssetClassPath.ToString()));
		};
	
		ClassPickerFunc(AWHActor::StaticClass());
		ClassPickerFunc(APawnBase::StaticClass());
		ClassPickerFunc(ACharacterBase::StaticClass());
		ClassPickerFunc(AWHPlayerController::StaticClass());
		ClassPickerFunc(AWHGameMode::StaticClass());
		ClassPickerFunc(UActorComponent::StaticClass());
		ClassPickerFunc(USceneComponent::StaticClass());
	}
	
	return DefaultClassPickers;
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

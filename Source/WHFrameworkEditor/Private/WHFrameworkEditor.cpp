// Copyright Epic Games, Inc. All Rights Reserved.

#include "WHFrameworkEditor.h"

#include "AssetToolsModule.h"
#include "ISettingsModule.h"
#include "LevelEditor.h"
#include "Widgets/Docking/SDockTab.h"
#include "ToolMenus.h"
#include "UnrealEdGlobals.h"
#include "WHFrameworkCoreActions.h"
#include "WHFrameworkEditorActions.h"
#include "WHFrameworkEditorStyle.h"
#include "WHFrameworkEditorTypes.h"
#include "Achievement/AchievementEditor.h"
#include "Asset/AssetEditor.h"
#include "Camera/CameraEditor.h"
#include "Character/Base/CharacterBase.h"
#include "Editor/UnrealEdEngine.h"
#include "FSM/FSMEditor.h"
#include "Gameplay/WHGameMode.h"
#include "Main/MainEditorTypes.h"
#include "Main/MainManager.h"
#include "Parameter/ParameterEditor.h"
#include "Pawn/Base/PawnBase.h"
#include "Preferences/UnrealEdOptions.h"
#include "Procedure/ProcedureEditor.h"
#include "Scene/SceneEditor.h"
#include "Step/StepEditor.h"
#include "Task/TaskEditor.h"

#define LOCTEXT_NAMESPACE "FWHFrameworkEditorModule"

void FWHFrameworkEditorModule::StartupModule()
{
	// Register delegates
	BeginPIEDelegateHandle = FEditorDelegates::PostPIEStarted.AddRaw(this, &FWHFrameworkEditorModule::OnBeginPIE);
	EndPIEDelegateHandle = FEditorDelegates::EndPIE.AddRaw(this, &FWHFrameworkEditorModule::OnEndPIE);

	FCoreDelegates::OnPostEngineInit.AddRaw(this, &FWHFrameworkEditorModule::OnPostEngineInit);

	// Register styles
	FWHFrameworkEditorStyle::Initialize();

	// Register commands
	FWHFrameworkEditorCommands::Register();

	// Startup modules
	StartupEditorModules();

	// Register menus
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FWHFrameworkEditorModule::RegisterMenus));
}

void FWHFrameworkEditorModule::ShutdownModule()
{
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

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));

	#define STARTUP_MODULE(ModuleName) \
	{ \
		ModuleName::Get().StartupModule();\
		ModuleName::Get().RegisterSettings(SettingsModule); \
		ModuleName::Get().RegisterCommands(PluginCommands);\
		ModuleName::Get().RegisterAssetTypeAction(AssetTools, AssetCategory, CreatedAssetTypeActions);\
		ModuleName::Get().RegisterCustomization(PropertyEditorModule);\
	}

	STARTUP_MODULE(FAchievementEditorModule);
	STARTUP_MODULE(FAssetEditorModule);
	STARTUP_MODULE(FCameraEditorModule);
	STARTUP_MODULE(FFSMEditorModule);
	STARTUP_MODULE(FMainEditorModule);
	STARTUP_MODULE(FParameterEditorModule);
	STARTUP_MODULE(FProcedureEditorModule);
	STARTUP_MODULE(FSceneEditorModule);
	STARTUP_MODULE(FStepEditorModule);
	STARTUP_MODULE(FTaskEditorModule);

	PropertyEditorModule.NotifyCustomizationModuleChanged();
}

void FWHFrameworkEditorModule::ShutdownEditorModules()
{
	ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>(FName("Settings"));

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));

	#define SHUTDOWN_MODULE(ModuleName) \
	{ \
		ModuleName::Get().ShutdownModule();\
		ModuleName::Get().UnRegisterSettings(SettingsModule); \
		ModuleName::Get().UnRegisterCustomization(PropertyEditorModule);\
	}

	SHUTDOWN_MODULE(FAchievementEditorModule);
	SHUTDOWN_MODULE(FAssetEditorModule);
	SHUTDOWN_MODULE(FCameraEditorModule);
	SHUTDOWN_MODULE(FFSMEditorModule);
	SHUTDOWN_MODULE(FMainEditorModule);
	SHUTDOWN_MODULE(FParameterEditorModule);
	SHUTDOWN_MODULE(FProcedureEditorModule);
	SHUTDOWN_MODULE(FSceneEditorModule);
	SHUTDOWN_MODULE(FStepEditorModule);
	SHUTDOWN_MODULE(FTaskEditorModule);

	if(FModuleManager::Get().IsModuleLoaded(TEXT("AssetTools")))
	{
		IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();

		for(auto& AssetTypeAction : CreatedAssetTypeActions)
		{
			if(AssetTypeAction.IsValid())
			{
				AssetTools.UnregisterAssetTypeActions(AssetTypeAction.ToSharedRef());
			}
		}
	}
	CreatedAssetTypeActions.Empty();

	PropertyEditorModule.NotifyCustomizationModuleChanged();
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
	FAssetEditorModule::Get().RegisterMenus(PluginCommands);
	FCameraEditorModule::Get().RegisterMenus(PluginCommands);
	FFSMEditorModule::Get().RegisterMenus(PluginCommands);
	FMainEditorModule::Get().RegisterMenus(PluginCommands);
	FProcedureEditorModule::Get().RegisterMenus(PluginCommands);
	FParameterEditorModule::Get().RegisterMenus(PluginCommands);
	FSceneEditorModule::Get().RegisterMenus(PluginCommands);
	FStepEditorModule::Get().RegisterMenus(PluginCommands);
	FTaskEditorModule::Get().RegisterMenus(PluginCommands);

	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
	LevelEditorModule.GetGlobalLevelEditorActions()->Append(PluginCommands.ToSharedRef());

	TSharedRef<FUICommandList> LevelEditorActions = LevelEditorModule.GetGlobalLevelEditorActions();

	FWHFrameworkEditorCommands::MapActions(LevelEditorActions);
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
		
		GUnrealEd->OnBlueprintCompiled().AddRaw(this, &FWHFrameworkEditorModule::OnBlueprintCompiled);

		GUnrealEd->OnLevelViewportClientListChanged().AddRaw(this, &FWHFrameworkEditorModule::OnLevelViewportClientListChanged);
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

void FWHFrameworkEditorModule::OnBlueprintCompiled()
{
	if(FWHFrameworkCoreDelegates::OnBlueprintCompiled.IsBound())
	{
		FWHFrameworkCoreDelegates::OnBlueprintCompiled.Broadcast();
	}
}

void FWHFrameworkEditorModule::OnLevelViewportClientListChanged()
{
	if(FWHFrameworkCoreDelegates::OnViewportListChanged.IsBound())
	{
		FWHFrameworkCoreDelegates::OnViewportListChanged.Broadcast();
	}
}

void FWHFrameworkEditorModule::OnBeginPIE(bool bIsSimulating)
{
	GIsPlaying = true;
	GIsSimulating = bIsSimulating;

	GWorldContext = GetMutableWorldContext(false);
}

void FWHFrameworkEditorModule::OnEndPIE(bool bIsSimulating)
{
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float InDeltaTime)
	{
		GIsPlaying = false;
		GIsSimulating = false;

		GWorldContext = GetMutableWorldContext(true);

		for(auto Iter : FMainManager::GetAllManager())
		{
			Iter->OnReset();
		}
		
		return false;
	}));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FWHFrameworkEditorModule, WHFrameworkEditor)

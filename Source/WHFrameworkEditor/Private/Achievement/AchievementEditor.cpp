// Copyright Epic Games, Inc. All Rights Reserved.

#include "Achievement/AchievementEditor.h"

#include "Editor.h"
#define LOCTEXT_NAMESPACE "FAchievementEditor"

IMPLEMENTATION_EDITOR_MODULE(FAchievementEditorModule)

FAchievementEditorModule::FAchievementEditorModule()
{
	AppIdentifier = FName("AchievementEditorApp");
}

void FAchievementEditorModule::StartupModule()
{
	
}

void FAchievementEditorModule::ShutdownModule()
{
	
}

void FAchievementEditorModule::RegisterSettings(ISettingsModule* SettingsModule)
{
	
}

void FAchievementEditorModule::UnRegisterSettings(ISettingsModule* SettingsModule)
{
	
}

bool FAchievementEditorModule::HandleSettingsSaved()
{
	return true;
}

void FAchievementEditorModule::RegisterCommands(const TSharedPtr<FUICommandList>& Commands)
{
	
}

void FAchievementEditorModule::RegisterMenus(const TSharedPtr<FUICommandList>& Commands)
{
	
}

void FAchievementEditorModule::RegisterAssetTypeAction(IAssetTools& AssetTools, EAssetTypeCategories::Type AssetCategory, TArray<TSharedPtr<IAssetTypeActions>>& AssetTypeActions)
{
	
}

void FAchievementEditorModule::RegisterCustomClassLayout(FPropertyEditorModule& PropertyEditor)
{
	
}

void FAchievementEditorModule::UnRegisterCustomClassLayout(FPropertyEditorModule& PropertyEditor)
{
	PropertyEditor.UnregisterCustomClassLayout(FName("AchievementBase"));
}

#undef LOCTEXT_NAMESPACE

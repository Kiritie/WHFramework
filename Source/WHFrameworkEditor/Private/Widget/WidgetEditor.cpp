// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widget/WidgetEditor.h"

#include "Editor.h"
#define LOCTEXT_NAMESPACE "FWidgetEditor"

IMPLEMENTATION_EDITOR_MODULE(FWidgetEditorModule)

FWidgetEditorModule::FWidgetEditorModule()
{
	AppIdentifier = FName("WidgetEditorApp");
}

void FWidgetEditorModule::StartupModule()
{
	
}

void FWidgetEditorModule::ShutdownModule()
{
	
}

void FWidgetEditorModule::RegisterSettings(ISettingsModule* SettingsModule)
{
	
}

void FWidgetEditorModule::UnRegisterSettings(ISettingsModule* SettingsModule)
{
	
}

bool FWidgetEditorModule::HandleSettingsSaved()
{
	return true;
}

void FWidgetEditorModule::RegisterCommands(const TSharedPtr<FUICommandList>& Commands)
{
	
}

void FWidgetEditorModule::RegisterMenus(const TSharedPtr<FUICommandList>& Commands)
{
	
}

void FWidgetEditorModule::RegisterAssetTypeAction(IAssetTools& AssetTools, EAssetTypeCategories::Type AssetCategory, TArray<TSharedPtr<IAssetTypeActions>>& AssetTypeActions)
{
	
}

void FWidgetEditorModule::RegisterCustomClassLayout(FPropertyEditorModule& PropertyEditor)
{
	
}

void FWidgetEditorModule::UnRegisterCustomClassLayout(FPropertyEditorModule& PropertyEditor)
{
	PropertyEditor.UnregisterCustomClassLayout(FName("WidgetBase"));
}

#undef LOCTEXT_NAMESPACE

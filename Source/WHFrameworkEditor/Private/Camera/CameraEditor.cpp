// Copyright Epic Games, Inc. All Rights Reserved.

#include "Camera/CameraEditor.h"

#include "Editor.h"
#include "Camera/Customization/CameraModuleDetailCustomization.h"
#include "Camera/Customization/CameraPointDetailCustomization.h"

#define LOCTEXT_NAMESPACE "FCameraEditor"

IMPLEMENTATION_EDITOR_MODULE(FCameraEditorModule)

FCameraEditorModule::FCameraEditorModule()
{
	AppIdentifier = FName("CameraEditorApp");
}

void FCameraEditorModule::StartupModule()
{
	
}

void FCameraEditorModule::ShutdownModule()
{
	
}

void FCameraEditorModule::RegisterSettings(ISettingsModule* SettingsModule)
{
	
}

void FCameraEditorModule::UnRegisterSettings(ISettingsModule* SettingsModule)
{

}

bool FCameraEditorModule::HandleSettingsSaved()
{
	return true;
}

void FCameraEditorModule::RegisterCommands(const TSharedPtr<FUICommandList>& Commands)
{

}

void FCameraEditorModule::RegisterMenus(const TSharedPtr<FUICommandList>& Commands)
{

}

void FCameraEditorModule::RegisterAssetTypeAction(IAssetTools& AssetTools, EAssetTypeCategories::Type AssetCategory, TArray<TSharedPtr<IAssetTypeActions>>& AssetTypeActions)
{

}

void FCameraEditorModule::RegisterCustomClassLayout(FPropertyEditorModule& PropertyEditor)
{
	PropertyEditor.RegisterCustomClassLayout(FName("CameraModule"), FOnGetDetailCustomizationInstance::CreateStatic(&FCameraModuleDetailCustomization::MakeInstance));
	PropertyEditor.RegisterCustomClassLayout(FName("CameraPointBase"), FOnGetDetailCustomizationInstance::CreateStatic(&FCameraPointDetailCustomization::MakeInstance));
}

void FCameraEditorModule::UnRegisterCustomClassLayout(FPropertyEditorModule& PropertyEditor)
{
	PropertyEditor.UnregisterCustomClassLayout(FName("CameraModule"));
	PropertyEditor.UnregisterCustomClassLayout(FName("CameraPointBase"));
}

#undef LOCTEXT_NAMESPACE

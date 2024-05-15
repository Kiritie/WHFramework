// Copyright Epic Games, Inc. All Rights Reserved.

#include "Parameter/ParameterEditor.h"

#include "Editor.h"
#include "Parameter/Customization/ParameterCustomization.h"
#include "Parameter/Customization/Value/EnumParameterValueCustomization.h"

#define LOCTEXT_NAMESPACE "FParameterEditor"

IMPLEMENTATION_EDITOR_MODULE(FParameterEditorModule)

FParameterEditorModule::FParameterEditorModule()
{
	AppIdentifier = FName("ParameterEditorApp");
}

void FParameterEditorModule::StartupModule()
{
	
}

void FParameterEditorModule::ShutdownModule()
{
	
}

void FParameterEditorModule::RegisterSettings(ISettingsModule* SettingsModule)
{
	
}

void FParameterEditorModule::UnRegisterSettings(ISettingsModule* SettingsModule)
{

}

bool FParameterEditorModule::HandleSettingsSaved()
{
	return true;
}

void FParameterEditorModule::RegisterCommands(const TSharedPtr<FUICommandList>& Commands)
{

}

void FParameterEditorModule::RegisterMenus(const TSharedPtr<FUICommandList>& Commands)
{

}

void FParameterEditorModule::RegisterAssetTypeAction(IAssetTools& AssetTools, EAssetTypeCategories::Type AssetCategory, TArray<TSharedPtr<IAssetTypeActions>>& AssetTypeActions)
{

}

void FParameterEditorModule::RegisterCustomization(FPropertyEditorModule& PropertyEditor)
{
	PropertyEditor.RegisterCustomPropertyTypeLayout(FName("Parameter"), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FParameterCustomization::MakeInstance));
	PropertyEditor.RegisterCustomPropertyTypeLayout(FName("EnumParameterValue"), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FEnumParameterValueCustomization::MakeInstance));
}

void FParameterEditorModule::UnRegisterCustomization(FPropertyEditorModule& PropertyEditor)
{
	PropertyEditor.UnregisterCustomPropertyTypeLayout(FName("Parameter"));
	PropertyEditor.UnregisterCustomPropertyTypeLayout(FName("EnumParameterValue"));
}

#undef LOCTEXT_NAMESPACE

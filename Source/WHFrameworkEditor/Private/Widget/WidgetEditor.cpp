// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widget/WidgetEditor.h"
#include "Widget/Theme/WidgetThemeToolkit.h"
#include "Widget/Theme/WidgetThemeDesignerIntegration.h"
#include "Widget/Theme/WidgetThemeEditorSettings.h"

#include "Widget/Customization/WidgetModuleCustomization.h"
#include "Widget/Customization/WidgetStyleCustomization.h"
#include "Widget/Customization/WidgetThemeCustomization.h"

IMPLEMENTATION_EDITOR_MODULE(FWidgetEditorModule)

FWidgetEditorModule::FWidgetEditorModule()
{
	AppIdentifier = FName("WidgetEditorApp");
}

void FWidgetEditorModule::StartupModule()
{
	if (IsRunningCommandlet())
	{
		return;
	}
	UWidgetThemeEditorSettings::ApplyFolderVisibility();
	RegisterWidgetThemeAssetEditor();
	RegisterWidgetThemeDesignerIntegration();
}

void FWidgetEditorModule::ShutdownModule()
{
	UWidgetThemeEditorSettings::ClearFolderVisibility();
	if (IsRunningCommandlet())
	{
		return;
	}
	UnregisterWidgetThemeDesignerIntegration();
	UnregisterWidgetThemeAssetEditor();
}

void FWidgetEditorModule::RegisterCustomization(FPropertyEditorModule& PropertyEditor)
{
	PropertyEditor.RegisterCustomClassLayout(FName("WidgetModule"), FOnGetDetailCustomizationInstance::CreateStatic(&FWidgetModuleCustomization::MakeInstance));
	PropertyEditor.RegisterCustomClassLayout(FName("CommonButton"),
											 FOnGetDetailCustomizationInstance::CreateStatic(&FWidgetStyleCustomization::MakeButtonInstance));
	PropertyEditor.RegisterCustomClassLayout(FName("CommonTextBlockN"),
											 FOnGetDetailCustomizationInstance::CreateStatic(&FWidgetStyleCustomization::MakeTextInstance));
	PropertyEditor.RegisterCustomClassLayout(FName("CommonImageN"),
											 FOnGetDetailCustomizationInstance::CreateStatic(&FWidgetStyleCustomization::MakeImageInstance));
	PropertyEditor.RegisterCustomClassLayout(FName("WidgetTheme"), FOnGetDetailCustomizationInstance::CreateStatic(&FWidgetThemeCustomization::MakeInstance));
}

void FWidgetEditorModule::UnRegisterCustomization(FPropertyEditorModule& PropertyEditor)
{
	PropertyEditor.UnregisterCustomClassLayout(FName("WidgetModule"));
	PropertyEditor.UnregisterCustomClassLayout(FName("CommonButton"));
	PropertyEditor.UnregisterCustomClassLayout(FName("CommonTextBlockN"));
	PropertyEditor.UnregisterCustomClassLayout(FName("CommonImageN"));
	PropertyEditor.UnregisterCustomClassLayout(FName("WidgetTheme"));
}

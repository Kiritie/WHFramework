// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widget/WidgetEditor.h"
#include "Widget/WidgetEditorToolset.h"
#include "ToolsetRegistry/UToolsetRegistry.h"
#include "Misc/CoreDelegates.h"
#include "Widget/Theme/WidgetThemeToolkit.h"
#include "Widget/Theme/WidgetThemeDesignerIntegration.h"
#include "Widget/Theme/WidgetThemeEditorSettings.h"
#include "Widget/Theme/WidgetThemeResolver.h"

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
	FCoreDelegates::GetOnPostEngineInit().AddRaw(this, &FWidgetEditorModule::InitializeDefaultTheme);
	RegisterWidgetThemeAssetEditor();
	RegisterWidgetThemeDesignerIntegration();
	if (UToolsetRegistry::IsAvailable())
	{
		RegisterToolsets();
	}
	else
	{
		FCoreDelegates::OnAllModuleLoadingPhasesComplete.AddRaw(this, &FWidgetEditorModule::RegisterToolsets);
	}
}

void FWidgetEditorModule::ShutdownModule()
{
	FCoreDelegates::OnAllModuleLoadingPhasesComplete.RemoveAll(this);
	FCoreDelegates::GetOnPostEngineInit().RemoveAll(this);
	UWidgetThemeEditorSettings::ClearFolderVisibility();
	if (IsRunningCommandlet())
	{
		return;
	}
	UnregisterWidgetThemeDesignerIntegration();
	UnregisterWidgetThemeAssetEditor();
	if (UToolsetRegistry::IsAvailable())
	{
		UToolsetRegistry::UnregisterToolsetClass(UWidgetEditorToolset::StaticClass());
	}
}

void FWidgetEditorModule::RegisterToolsets()
{
	UToolsetRegistry::RegisterToolsetClass(UWidgetEditorToolset::StaticClass());
}

void FWidgetEditorModule::InitializeDefaultTheme()
{
	FWidgetThemeResolver::ResolveTheme(nullptr);
	FWidgetThemeResolver::NotifyThemeChanged();
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
	PropertyEditor.RegisterCustomClassLayout(FName("CommonBorderN"),
												 FOnGetDetailCustomizationInstance::CreateStatic(&FWidgetStyleCustomization::MakeBorderInstance));
	PropertyEditor.RegisterCustomClassLayout(FName("CommonRichTextBlockN"),
												 FOnGetDetailCustomizationInstance::CreateStatic(&FWidgetStyleCustomization::MakeRichTextInstance));
	PropertyEditor.RegisterCustomClassLayout(FName("CommonScroll"),
											 FOnGetDetailCustomizationInstance::CreateStatic(&FWidgetStyleCustomization::MakeScrollInstance));
	PropertyEditor.RegisterCustomClassLayout(FName("CommonButtonStyleN"),
												 FOnGetDetailCustomizationInstance::CreateStatic(&FWidgetStyleCustomization::MakeButtonStyleInstance));
	PropertyEditor.RegisterCustomClassLayout(FName("WidgetTheme"), FOnGetDetailCustomizationInstance::CreateStatic(&FWidgetThemeCustomization::MakeInstance));
}

void FWidgetEditorModule::UnRegisterCustomization(FPropertyEditorModule& PropertyEditor)
{
	PropertyEditor.UnregisterCustomClassLayout(FName("WidgetModule"));
	PropertyEditor.UnregisterCustomClassLayout(FName("CommonButton"));
	PropertyEditor.UnregisterCustomClassLayout(FName("CommonTextBlockN"));
	PropertyEditor.UnregisterCustomClassLayout(FName("CommonImageN"));
	PropertyEditor.UnregisterCustomClassLayout(FName("CommonBorderN"));
	PropertyEditor.UnregisterCustomClassLayout(FName("CommonRichTextBlockN"));
	PropertyEditor.UnregisterCustomClassLayout(FName("CommonScroll"));
	PropertyEditor.UnregisterCustomClassLayout(FName("CommonButtonStyleN"));
	PropertyEditor.UnregisterCustomClassLayout(FName("WidgetTheme"));
}

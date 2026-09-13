// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widget/WidgetEditor.h"

#include "Widget/Customization/WidgetModuleCustomization.h"
#include "Widget/Customization/WidgetStyleCustomization.h"

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

void FWidgetEditorModule::RegisterCustomization(FPropertyEditorModule& PropertyEditor)
{
	PropertyEditor.RegisterCustomClassLayout(
		FName("WidgetModule"),
		FOnGetDetailCustomizationInstance::CreateStatic(&FWidgetModuleCustomization::MakeInstance));
	PropertyEditor.RegisterCustomClassLayout(
		FName("CommonButton"),
		FOnGetDetailCustomizationInstance::CreateStatic(&FWidgetStyleCustomization::MakeButtonInstance));
	PropertyEditor.RegisterCustomClassLayout(
		FName("CommonTextBlockN"),
		FOnGetDetailCustomizationInstance::CreateStatic(&FWidgetStyleCustomization::MakeTextInstance));
	PropertyEditor.RegisterCustomClassLayout(
		FName("CommonImageN"),
		FOnGetDetailCustomizationInstance::CreateStatic(&FWidgetStyleCustomization::MakeImageInstance));
}

void FWidgetEditorModule::UnRegisterCustomization(FPropertyEditorModule& PropertyEditor)
{
	PropertyEditor.UnregisterCustomClassLayout(FName("WidgetModule"));
	PropertyEditor.UnregisterCustomClassLayout(FName("CommonButton"));
	PropertyEditor.UnregisterCustomClassLayout(FName("CommonTextBlockN"));
	PropertyEditor.UnregisterCustomClassLayout(FName("CommonImageN"));
}

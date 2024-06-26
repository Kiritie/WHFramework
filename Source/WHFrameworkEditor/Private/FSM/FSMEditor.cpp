// Copyright Epic Games, Inc. All Rights Reserved.

#include "FSM/FSMEditor.h"

#include "Editor.h"
#include "FSM/Base/FiniteStateBlueprint.h"
#include "FSM/Blueprint/FiniteStateBlueprintActions.h"
#include "FSM/Blueprint/FiniteStateGraphSchema.h"
#include "FSM/Customization/FSMComponentCustomization.h"

#define LOCTEXT_NAMESPACE "FFSMEditor"

//////////////////////////////////////////////////////////////////////////
/// FSMEditorModule
IMPLEMENTATION_EDITOR_MODULE(FFSMEditorModule)

FFSMEditorModule::FFSMEditorModule()
{
	AppIdentifier = FName("FSMEditorApp");
}

void FFSMEditorModule::StartupModule()
{
	
}

void FFSMEditorModule::ShutdownModule()
{
	
}

void FFSMEditorModule::RegisterSettings(ISettingsModule* SettingsModule)
{
	
}

void FFSMEditorModule::UnRegisterSettings(ISettingsModule* SettingsModule)
{

}

bool FFSMEditorModule::HandleSettingsSaved()
{
	return true;
}

void FFSMEditorModule::RegisterCommands(const TSharedPtr<FUICommandList>& Commands)
{

}

void FFSMEditorModule::RegisterMenus(const TSharedPtr<FUICommandList>& Commands)
{

}

void FFSMEditorModule::RegisterAssetTypeAction(IAssetTools& AssetTools, EAssetTypeCategories::Type AssetCategory, TArray<TSharedPtr<IAssetTypeActions>>& AssetTypeActions)
{
	const TSharedRef<IAssetTypeActions> AssetAction = MakeShareable(new FFiniteStateBlueprintActions(AssetCategory));
	AssetTools.RegisterAssetTypeActions(AssetAction);
	AssetTypeActions.Add(AssetAction);
}

void FFSMEditorModule::RegisterCustomization(FPropertyEditorModule& PropertyEditor)
{
	FEditorModuleBase::RegisterCustomization(PropertyEditor);
	
	PropertyEditor.RegisterCustomClassLayout(FName("FSMComponent"), FOnGetDetailCustomizationInstance::CreateStatic(&FFSMComponentCustomization::MakeInstance));
}

void FFSMEditorModule::UnRegisterCustomization(FPropertyEditorModule& PropertyEditor)
{
	PropertyEditor.UnregisterCustomClassLayout(FName("FSMComponent"));
}

//////////////////////////////////////////////////////////////////////////
/// FiniteStateBlueprintEditor
FFiniteStateBlueprintEditor::FFiniteStateBlueprintEditor()
{
	BlueprintClass = UFiniteStateBlueprint::StaticClass();
	GraphSchemaClass = UFiniteStateGraphSchema::StaticClass();
	
	ToolkitFName = FName("FiniteStateBlueprintEditor");
	BaseToolkitName = LOCTEXT("FiniteStateBlueprintEditorAppLabel", "FSM Blueprint Editor");
	ToolkitNameFormat = LOCTEXT("FiniteStateBlueprintEditorToolkitName", "{ObjectName}{DirtyState}");

	WorldCentricTabPrefix = TEXT("FiniteStateBlueprintEditor");
	WorldCentricTabColorScale = FLinearColor::White;
}

#undef LOCTEXT_NAMESPACE

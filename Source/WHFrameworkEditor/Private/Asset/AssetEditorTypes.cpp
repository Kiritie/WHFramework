// Fill out your copyright notice in the Description page of Project Settings.

#include "Asset/AssetEditorTypes.h"

#include "Asset/Modifier/AssetModifierBase.h"

#define LOCTEXT_NAMESPACE "AssetEditorTypes"

//////////////////////////////////////////////////////////////////////////
// Commands
void FAssetModifierEditorCommands::RegisterCommands()
{
	UI_COMMAND(OpenAssetModifierEditorWindow, "Asset Modifier Editor", "Bring up AssetModifierEditor window", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::T));
}

//////////////////////////////////////////////////////////////////////////
// ClassFilter
FAssetModifierClassFilter::FAssetModifierClassFilter()
{
	DisallowedClassFlags = CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists | CLASS_HideDropDown;
	AllowedChildrenOfClasses.Add(UAssetModifierBase::StaticClass());
	bIncludeBaseClasses = false;
	bIncludeParentClasses = false;
}

bool FAssetModifierClassFilter::IsClassAllowed(UClass* InClass)
{
	return FClassViewerFilterBase::IsClassAllowed(InClass);
}

//////////////////////////////////////////////////////////////////////////
// EditorSettings
UAssetModifierEditorSettings::UAssetModifierEditorSettings()
{
}

#undef LOCTEXT_NAMESPACE

// Fill out your copyright notice in the Description page of Project Settings.


#include "Main/MainEditorTypes.h"

#include "Main/MainModule.h"

#define LOCTEXT_NAMESPACE "ModuleEditorTypes"

//////////////////////////////////////////////////////////////////////////
// ClassFilter
FModuleClassFilter::FModuleClassFilter()
{
	DisallowedClassFlags = CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists | CLASS_HideDropDown;
	AllowedChildrenOfClasses.Add(UModuleBase::StaticClass());
	bIncludeBaseClasses = false;
	bIncludeParentClasses = false;
		
	MainModule = nullptr;
}

bool FModuleClassFilter::IsClassAllowed(const UClass* InClass)
{
	return !MainModule->GetModuleMap().Contains(InClass->GetDefaultObject<UModuleBase>()->GetModuleName());
}

//////////////////////////////////////////////////////////////////////////
// Commands
void FModuleEditorCommands::RegisterCommands()
{
	UI_COMMAND(OpenModuleEditorWindow, "Module Editor", "Bring up ModuleEditor window", EUserInterfaceActionType::Button, FInputGesture());

	FUICommandInfo::MakeCommandInfo(AsShared(), this->Save, "Save", FText::FromString("Save"), FText(), FSlateIcon(), EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::S));
}

//////////////////////////////////////////////////////////////////////////
// EditorSettings
UModuleEditorSettings::UModuleEditorSettings()
{
	bDefaultIsMultiMode = false;
	bDefaultIsEditMode = false;
}

#undef LOCTEXT_NAMESPACE

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
	
	MainModule = nullptr;
	EditingModuleClass = nullptr;
}

bool FModuleClassFilter::IsClassAllowed(UClass* InClass)
{
	return EditingModuleClass ? InClass != EditingModuleClass && InClass->GetDefaultObject<UModuleBase>()->GetModuleName() == EditingModuleClass.GetDefaultObject()->GetModuleName() : MainModule->CanAddModule(InClass);
}

//////////////////////////////////////////////////////////////////////////
// Commands
void FModuleEditorCommands::RegisterCommands()
{
	UI_COMMAND(OpenModuleEditorWindow, "Module Editor", "Bring up ModuleEditor window", EUserInterfaceActionType::Button, FInputGesture());

	FUICommandInfo::MakeCommandInfo(AsShared(), SaveModuleEditor, FName("SaveModuleEditor"),
		LOCTEXT("SaveModuleEditor", "Save"),
		LOCTEXT("SaveModuleEditorTooltip", "Save ModuleEditor"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Level.SaveIcon16x"),
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Control, EKeys::S));
}

//////////////////////////////////////////////////////////////////////////
// EditorSettings
UModuleEditorSettings::UModuleEditorSettings()
{
	bDefaultIsDefaults = false;
	bDefaultIsEditing = false;
}

#undef LOCTEXT_NAMESPACE

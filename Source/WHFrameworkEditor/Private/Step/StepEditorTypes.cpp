// Copyright Epic Games, Inc. All Rights Reserved.

#include "Step/StepEditorTypes.h"

#include "Step/Base/StepAsset.h"
#include "Step/Base/StepBase.h"

#define LOCTEXT_NAMESPACE "StepEditorTypes"

//////////////////////////////////////////////////////////////////////////
// ClassFilter
FStepClassFilter::FStepClassFilter()
{
	DisallowedClassFlags = CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists | CLASS_HideDropDown;
	AllowedChildrenOfClasses.Add(UStepBase::StaticClass());
	bIncludeBaseClasses = true;
	bIncludeParentClasses = true;
		
	StepEditor = nullptr;
}

bool FStepClassFilter::IsClassAllowed(UClass* InClass)
{
	return StepEditor.Pin()->GetEditingAsset<UStepAsset>()->CanAddStep(InClass);
}

//////////////////////////////////////////////////////////////////////////
// Commands
void FStepEditorCommands::RegisterCommands()
{
	UI_COMMAND(OpenStepEditorWindow, "Step Editor", "Bring up StepEditor window", EUserInterfaceActionType::Button, FInputGesture());
}

//////////////////////////////////////////////////////////////////////////
// EditorSettings
UStepEditorSettings::UStepEditorSettings()
{
	bDefaultIsDefaults = false;
	bDefaultIsEditing = false;
}

#undef LOCTEXT_NAMESPACE

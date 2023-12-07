// Copyright Epic Games, Inc. All Rights Reserved.

#include "Task/TaskEditorTypes.h"

#include "Task/Base/TaskAsset.h"
#include "Task/Base/TaskBase.h"

#define LOCTEXT_NAMESPACE "TaskEditorTypes"

//////////////////////////////////////////////////////////////////////////
// ClassFilter
FTaskClassFilter::FTaskClassFilter()
{
	DisallowedClassFlags = CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists | CLASS_HideDropDown;
	AllowedChildrenOfClasses.Add(UTaskBase::StaticClass());
	bIncludeBaseClasses = true;
	bIncludeParentClasses = true;
		
	TaskEditor = nullptr;
}

bool FTaskClassFilter::IsClassAllowed(UClass* InClass)
{
	return TaskEditor.Pin()->GetEditingAsset<UTaskAsset>()->CanAddTask(InClass);
}

//////////////////////////////////////////////////////////////////////////
// Commands
void FTaskEditorCommands::RegisterCommands()
{
	UI_COMMAND(OpenTaskEditorWindow, "Task Editor", "Bring up TaskEditor window", EUserInterfaceActionType::Button, FInputGesture());
}

//////////////////////////////////////////////////////////////////////////
// EditorSettings
UTaskEditorSettings::UTaskEditorSettings()
{
	bDefaultIsDefaults = false;
	bDefaultIsEditing = false;
}

#undef LOCTEXT_NAMESPACE

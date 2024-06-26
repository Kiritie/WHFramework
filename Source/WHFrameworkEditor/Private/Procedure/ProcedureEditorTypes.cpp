// Copyright Epic Games, Inc. All Rights Reserved.

#include "Procedure/ProcedureEditorTypes.h"

#include "Procedure/Base/ProcedureAsset.h"
#include "Procedure/Base/ProcedureBase.h"

#define LOCTEXT_NAMESPACE "ProcedureEditorTypes"

//////////////////////////////////////////////////////////////////////////
// ClassFilter
FProcedureClassFilter::FProcedureClassFilter()
{
	DisallowedClassFlags = CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists | CLASS_HideDropDown;
	AllowedChildrenOfClasses.Add(UProcedureBase::StaticClass());
	bIncludeBaseClasses = false;
	bIncludeParentClasses = false;
		
	ProcedureEditor = nullptr;
}

bool FProcedureClassFilter::IsClassAllowed(UClass* InClass)
{
	return ProcedureEditor.Pin()->GetEditingAsset<UProcedureAsset>()->CanAddProcedure(InClass);
}

//////////////////////////////////////////////////////////////////////////
// Commands
void FProcedureEditorCommands::RegisterCommands()
{
	UI_COMMAND(OpenProcedureEditorWindow, "Procedure Editor", "Bring up ProcedureEditor window", EUserInterfaceActionType::Button, FInputGesture());
}

//////////////////////////////////////////////////////////////////////////
// EditorSettings
UProcedureEditorSettings::UProcedureEditorSettings()
{
	bDefaultIsDefaults = false;
	bDefaultIsEditing = false;
}

#undef LOCTEXT_NAMESPACE

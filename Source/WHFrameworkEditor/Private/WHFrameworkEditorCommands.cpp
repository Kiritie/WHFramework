// Copyright Epic Games, Inc. All Rights Reserved.

#include "WHFrameworkEditorCommands.h"

#define LOCTEXT_NAMESPACE "FWHFrameworkEditorModule"

void FWHFrameworkEditorCommands::RegisterCommands()
{
	UI_COMMAND(OpenModuleEditorWindow, "Module Editor", "Bring up ModuleEditor window", EUserInterfaceActionType::Button, FInputGesture());

	UI_COMMAND(OpenProcedureEditorWindow, "Procedure Editor", "Bring up ProcedureEditor window", EUserInterfaceActionType::Button, FInputGesture());

	UI_COMMAND(OpenStepEditorWindow, "Step Editor", "Bring up StepEditor window", EUserInterfaceActionType::Button, FInputGesture());

	UI_COMMAND(OpenTaskEditorWindow, "Task Editor", "Bring up TaskEditor window", EUserInterfaceActionType::Button, FInputGesture());
}

#undef LOCTEXT_NAMESPACE

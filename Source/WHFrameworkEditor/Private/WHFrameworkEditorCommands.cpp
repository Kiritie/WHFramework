// Copyright Epic Games, Inc. All Rights Reserved.

#include "WHFrameworkEditorCommands.h"

#define LOCTEXT_NAMESPACE "FWHFrameworkEditorModule"

void FWHFrameworkEditorCommands::RegisterCommands()
{
	UI_COMMAND(OpenProcedureEditorWindow, "Procedure Editor", "Bring up ProcedureEditor window", EUserInterfaceActionType::Button, FInputGesture());
}

#undef LOCTEXT_NAMESPACE

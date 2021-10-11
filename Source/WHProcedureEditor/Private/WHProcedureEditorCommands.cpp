// Copyright Epic Games, Inc. All Rights Reserved.

#include "WHProcedureEditorCommands.h"

#define LOCTEXT_NAMESPACE "FWHProcedureEditorModule"

void FWHProcedureEditorCommands::RegisterCommands()
{
	UI_COMMAND(OpenPluginWindow, "WHProcedureEditor", "Bring up WHProcedureEditor window", EUserInterfaceActionType::Button, FInputGesture());
}

#undef LOCTEXT_NAMESPACE

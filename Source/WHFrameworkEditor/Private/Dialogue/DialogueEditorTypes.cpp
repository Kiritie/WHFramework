#include "Dialogue/DialogueEditorTypes.h"

#define LOCTEXT_NAMESPACE "DialogueEditorCommands"

FDialogueEditorCommands::FDialogueEditorCommands()
	: TCommands<FDialogueEditorCommands>(TEXT("DialogueEditor"), LOCTEXT("Context", "Dialogue Editor"), NAME_None, FAppStyle::GetAppStyleSetName())
{
}

void FDialogueEditorCommands::RegisterCommands()
{
	UI_COMMAND(ValidateDialogue, "Validate", "Validate dialogue nodes and links", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "Styling/AppStyle.h"

struct FDialogueEditorState
{
	int32 CurrentNodeId = INDEX_NONE;
	bool isLinking = false;
	FVector2D LinkingCoords = FVector2D::ZeroVector;
	int32 LinkingFromNodeId = INDEX_NONE;
};

class FDialogueEditorCommands : public TCommands<FDialogueEditorCommands>
{
public:
	FDialogueEditorCommands();
	virtual void RegisterCommands() override;
	TSharedPtr<FUICommandInfo> ValidateDialogue;
};

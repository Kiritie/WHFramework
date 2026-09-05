#include "Dialogue/Interaction/InteractionAction_Dialogue.h"

#include "Dialogue/DialogueModule.h"
#include "Dialogue/DialogueModuleStatics.h"

bool UInteractionAction_Dialogue::Execute_Implementation(const FInteractionContext& InContext, FText& OutReason) const
{
	if (!UDialogueModule::IsValid()) return false;
	return UDialogueModuleStatics::StartDialogue(Dialogue.LoadSynchronous(), InContext.Player, InContext.Target);
}

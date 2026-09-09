#include "Dialogue/Interaction/InteractionAction_Dialogue.h"

#include "Dialogue/DialogueModule.h"
#include "Dialogue/DialogueModuleStatics.h"

void UInteractionAction_Dialogue::PostLoad()
{
	Super::PostLoad();
	ResolvedDialogue = Dialogue.LoadSynchronous();
}

bool UInteractionAction_Dialogue::Execute_Implementation(const FInteractionContext& InContext, FText& OutReason) const
{
	if (!UDialogueModule::IsValid()) return false;
	return UDialogueModuleStatics::StartDialogue(ResolvedDialogue ? ResolvedDialogue : Dialogue.Get(), InContext.Player, InContext.Target);
}

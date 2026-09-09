// Copyright Underflow Studios 2017

#include "Dialogue/Base/DialogueEventBase.h"
#include "Dialogue/Base/DialogueAsset.h"

UDialogueEventBase::UDialogueEventBase() {}

UWorld* UDialogueEventBase::GetWorld() const
{
	const UDialogueAsset* Dialogue = GetTypedOuter<UDialogueAsset>();
	return Dialogue ? Dialogue->GetWorld() : Super::GetWorld();
}

void UDialogueEventBase::ReceiveEventTriggered_Implementation(APlayerController* ConsideringPlayer, AActor* NPCActor) {}

bool UDialogueEventBase::Execute_Implementation(const FInteractionContext& InContext, FText& OutReason) const
{
	const_cast<UDialogueEventBase*>(this)->ReceiveEventTriggered(InContext.Player, InContext.Target);
	return true;
}

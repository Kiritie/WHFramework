// Copyright Underflow Studios 2017

#include "Dialogue/Base/DialogueConditionBase.h"
#include "Dialogue/Base/DialogueAsset.h"

UDialogueConditionBase::UDialogueConditionBase() {}

UWorld* UDialogueConditionBase::GetWorld() const
{
	const UDialogueAsset* Dialogue = GetTypedOuter<UDialogueAsset>();
	return Dialogue ? Dialogue->GetWorld() : Super::GetWorld();
}

bool UDialogueConditionBase::IsConditionMet_Implementation(APlayerController* ConsideringPlayer, AActor* NPCActor)
{
	return true;
}

bool UDialogueConditionBase::Evaluate_Implementation(const FInteractionContext& InContext, FText& OutReason) const
{
	return const_cast<UDialogueConditionBase*>(this)->IsConditionMet(InContext.Player, InContext.Target);
}

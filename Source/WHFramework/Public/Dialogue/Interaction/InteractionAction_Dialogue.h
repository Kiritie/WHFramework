#pragma once

#include "Common/Interaction/InteractionAction.h"
#include "InteractionAction_Dialogue.generated.h"

class UDialogueAsset;

UCLASS(EditInlineNew)
class WHFRAMEWORK_API UInteractionAction_Dialogue : public UInteractionAction
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UDialogueAsset> Dialogue;

	virtual bool Execute_Implementation(const FInteractionContext& InContext, FText& OutReason) const override;
};

#pragma once

#include "Common/Interaction/InteractionActionBase.h"
#include "InteractionAction_Dialogue.generated.h"

class UDialogueAsset;

UCLASS(EditInlineNew)
class WHFRAMEWORK_API UInteractionAction_Dialogue : public UInteractionActionBase
{
	GENERATED_BODY()

public:
	virtual void PostLoad() override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UDialogueAsset> Dialogue;

	UPROPERTY(Transient)
	UDialogueAsset* ResolvedDialogue = nullptr;

	virtual bool Execute_Implementation(const FInteractionContext& InContext, FText& OutReason) const override;
};

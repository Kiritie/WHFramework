#pragma once

#include "Common/Interaction/InteractionActionBase.h"
#include "GameplayTagContainer.h"
#include "InteractionAction_TaskObjective.generated.h"

UCLASS(EditInlineNew)
class WHFRAMEWORK_API UInteractionAction_TaskObjective : public UInteractionActionBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTag EventTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTag TargetTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1"))
	int32 Count = 1;

	virtual bool Execute_Implementation(const FInteractionContext& InContext, FText& OutReason) const override;
};

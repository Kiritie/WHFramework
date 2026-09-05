#pragma once

#include "Common/Interaction/InteractionTypes.h"
#include "UObject/Object.h"
#include "InteractionCondition.generated.h"

UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced)
class WHFRAMEWORK_API UInteractionCondition : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent)
	bool Evaluate(const FInteractionContext& InContext, FText& OutReason) const;

	virtual UWorld* GetWorld() const override;
};

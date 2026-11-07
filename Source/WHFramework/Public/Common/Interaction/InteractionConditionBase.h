#pragma once

#include "Common/InteractionTypes.h"
#include "Common/CommonTypes.h"
#include "UObject/Object.h"
#include "InteractionConditionBase.generated.h"

UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced)
class WHFRAMEWORK_API UInteractionConditionBase : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent)
	bool Evaluate(const FInteractionContext& InContext, FText& OutReason) const;

	virtual UWorld* GetWorld() const override;
};

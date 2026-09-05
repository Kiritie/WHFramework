#pragma once

#include "Common/Interaction/InteractionAction.h"
#include "Common/Interaction/InteractionCondition.h"
#include "Common/Interaction/InteractionOption.h"
#include "AbilityInteractionOptions.generated.h"

UCLASS(EditInlineNew)
class WHFRAMEWORK_API UInteractionCondition_AbilityRevive : public UInteractionCondition
{
	GENERATED_BODY()

public:
	virtual bool Evaluate_Implementation(const FInteractionContext& InContext, FText& OutReason) const override;
};

UCLASS(EditInlineNew)
class WHFRAMEWORK_API UInteractionAction_AbilityRevive : public UInteractionAction
{
	GENERATED_BODY()

public:
	virtual bool Execute_Implementation(const FInteractionContext& InContext, FText& OutReason) const override;
};

UCLASS(EditInlineNew)
class WHFRAMEWORK_API UInteractionOption_AbilityRevive : public UInteractionOption
{
	GENERATED_BODY()

public:
	UInteractionOption_AbilityRevive();
};

UCLASS(EditInlineNew)
class WHFRAMEWORK_API UInteractionCondition_AbilityPickUp : public UInteractionCondition
{
	GENERATED_BODY()

public:
	virtual bool Evaluate_Implementation(const FInteractionContext& InContext, FText& OutReason) const override;
};

UCLASS(EditInlineNew)
class WHFRAMEWORK_API UInteractionAction_AbilityPickUp : public UInteractionAction
{
	GENERATED_BODY()

public:
	virtual bool Execute_Implementation(const FInteractionContext& InContext, FText& OutReason) const override;
};

UCLASS(EditInlineNew)
class WHFRAMEWORK_API UInteractionOption_AbilityPickUp : public UInteractionOption
{
	GENERATED_BODY()

public:
	UInteractionOption_AbilityPickUp();
};

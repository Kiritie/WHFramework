#pragma once

#include "Common/Interaction/InteractionActionBase.h"
#include "Common/Interaction/InteractionConditionBase.h"
#include "Common/Interaction/InteractionOptionBase.h"
#include "AbilityInteractionOptions.generated.h"

UCLASS(EditInlineNew)
class WHFRAMEWORK_API UInteractionCondition_AbilityRevive : public UInteractionConditionBase
{
	GENERATED_BODY()

public:
	virtual bool Evaluate_Implementation(const FInteractionContext& InContext, FText& OutReason) const override;
};

UCLASS(EditInlineNew)
class WHFRAMEWORK_API UInteractionAction_AbilityRevive : public UInteractionActionBase
{
	GENERATED_BODY()

public:
	virtual bool Execute_Implementation(const FInteractionContext& InContext, FText& OutReason) const override;
};

UCLASS(EditInlineNew)
class WHFRAMEWORK_API UInteractionOption_AbilityRevive : public UInteractionOptionBase
{
	GENERATED_BODY()

public:
	UInteractionOption_AbilityRevive();
};

UCLASS(EditInlineNew)
class WHFRAMEWORK_API UInteractionCondition_AbilityPickUp : public UInteractionConditionBase
{
	GENERATED_BODY()

public:
	virtual bool Evaluate_Implementation(const FInteractionContext& InContext, FText& OutReason) const override;
};

UCLASS(EditInlineNew)
class WHFRAMEWORK_API UInteractionAction_AbilityPickUp : public UInteractionActionBase
{
	GENERATED_BODY()

public:
	virtual bool Execute_Implementation(const FInteractionContext& InContext, FText& OutReason) const override;
};

UCLASS(EditInlineNew)
class WHFRAMEWORK_API UInteractionOption_AbilityPickUp : public UInteractionOptionBase
{
	GENERATED_BODY()

public:
	UInteractionOption_AbilityPickUp();
};

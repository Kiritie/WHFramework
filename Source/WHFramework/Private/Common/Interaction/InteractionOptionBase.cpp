#include "Common/Interaction/InteractionOptionBase.h"

bool UInteractionOptionBase::IsVisible(const FInteractionContext& InContext) const
{
	FText Reason;
	for (const UInteractionConditionBase* Condition : VisibilityConditions)
	{
		if (!Condition || !Condition->Evaluate(InContext, Reason)) return false;
	}
	return true;
}

bool UInteractionOptionBase::IsEnabled(const FInteractionContext& InContext, FText& OutReason) const
{
	for (const UInteractionConditionBase* Condition : EnableConditions)
	{
		if (!Condition || !Condition->Evaluate(InContext, OutReason)) return false;
	}
	return !Actions.IsEmpty();
}

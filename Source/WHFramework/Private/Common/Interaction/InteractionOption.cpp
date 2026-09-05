#include "Common/Interaction/InteractionOption.h"

bool UInteractionOption::IsVisible(const FInteractionContext& InContext) const
{
	FText Reason;
	for (const UInteractionCondition* Condition : VisibilityConditions)
	{
		if (!Condition || !Condition->Evaluate(InContext, Reason)) return false;
	}
	return true;
}

bool UInteractionOption::IsEnabled(const FInteractionContext& InContext, FText& OutReason) const
{
	for (const UInteractionCondition* Condition : EnableConditions)
	{
		if (!Condition || !Condition->Evaluate(InContext, OutReason)) return false;
	}
	return !Actions.IsEmpty();
}

bool UInteractionOption::Execute(const FInteractionContext& InContext, FText& OutReason) const
{
	if (!IsVisible(InContext) || !IsEnabled(InContext, OutReason)) return false;
	for (const UInteractionAction* Action : Actions)
	{
		if (!Action || !Action->Execute(InContext, OutReason)) return false;
	}
	return true;
}

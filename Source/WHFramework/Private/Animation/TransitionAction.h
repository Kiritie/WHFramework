#pragma once

#include "LatentActions.h"

class FTransitionAction final : public FPendingLatentAction
{
public:
	FTransitionAction(float InDuration, const FLatentActionInfo& InInfo, UObject* InOwner)
		: Remaining(InDuration), ExecutionFunction(InInfo.ExecutionFunction), Link(InInfo.Linkage), Target(InInfo.CallbackTarget), Owner(InOwner) {}
	virtual void UpdateOperation(FLatentResponse& Response) override;
private:
	float Remaining;
	FName ExecutionFunction;
	int32 Link;
	FWeakObjectPtr Target;
	TWeakObjectPtr<UObject> Owner;
};

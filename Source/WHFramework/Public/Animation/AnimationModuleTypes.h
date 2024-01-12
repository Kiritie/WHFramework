// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "LatentActions.h"
#include "UObject/WeakObjectPtr.h"
#include "AnimationModuleTypes.generated.h"

//////////////////////////////////////////////////////////////////////////
/// CancelableDelay
UENUM(BlueprintType)
namespace ECancelableDelayAction
{
	enum Type
	{	
		None,
		Cancel
	};
}

class WHFRAMEWORK_API FCancelableDelayAction : public FPendingLatentAction
{
	public:
	float TimeRemaining;
	bool  bExecute;
	FName ExecutionFunction;
	int32 OutputLink;
	FWeakObjectPtr CallbackTarget;

	FCancelableDelayAction(float Duration, const FLatentActionInfo& LatentInfo)
		: TimeRemaining(Duration)
		, bExecute(true)
		, ExecutionFunction(LatentInfo.ExecutionFunction)
		, OutputLink(LatentInfo.Linkage)
		, CallbackTarget(LatentInfo.CallbackTarget)
	{
	}

	virtual void UpdateOperation(FLatentResponse& Response) override
	{
		if(bExecute)
		{
			TimeRemaining -= Response.ElapsedTime();
			Response.FinishAndTriggerIf(TimeRemaining <= 0.0f, ExecutionFunction, OutputLink, CallbackTarget);
		}
		else
		{
			Response.FinishAndTriggerIf(true, ExecutionFunction, OutputLink, nullptr);
		}
	}

#if WITH_EDITOR
	// Returns a human readable description of the latent operation's current state
	virtual FString GetDescription() const override
	{
		static const FNumberFormattingOptions CancelableDelayTimeFormatOptions = FNumberFormattingOptions()
			.SetMinimumFractionalDigits(3)
			.SetMaximumFractionalDigits(3);
		return FText::Format(NSLOCTEXT("CancelableDelayAction", "CancelableDelayActionTimeFmt", "Delay ({0} seconds left)"), FText::AsNumber(TimeRemaining, &CancelableDelayTimeFormatOptions)).ToString();
	}
#endif
};

//////////////////////////////////////////////////////////////////////////
/// Transition
class WHFRAMEWORK_API FTransitionAction : public FPendingLatentAction
{
public:
	float TimeRemaining;
	FName ExecutionFunction;
	int32 OutputLink;
	FWeakObjectPtr CallbackTarget;

	FTransitionAction(float Duration, const FLatentActionInfo& LatentInfo)
		: TimeRemaining(Duration)
		, ExecutionFunction(LatentInfo.ExecutionFunction)
		, OutputLink(LatentInfo.Linkage)
		, CallbackTarget(LatentInfo.CallbackTarget)
	{
	}

	virtual void UpdateOperation(FLatentResponse& Response) override
	{
		TimeRemaining -= Response.ElapsedTime();
		Response.FinishAndTriggerIf(TimeRemaining <= 0.0f, ExecutionFunction, OutputLink, CallbackTarget);
	}
};

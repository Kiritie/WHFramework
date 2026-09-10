#pragma once

#include "Parameter/ParameterModuleTypes.h"
#include "Event/EventModuleTypes.h"

#include "Event_StepStarted.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FEventStepStarted : public FEventBase
{
	GENERATED_BODY()

public:
	FEventStepStarted() = default;

	FEventStepStarted(int32 InRootStepIndex)
		: RootStepIndex(MoveTemp(InRootStepIndex))
	{
	}

	UPROPERTY(BlueprintReadWrite)
	int32 RootStepIndex = -1;
};

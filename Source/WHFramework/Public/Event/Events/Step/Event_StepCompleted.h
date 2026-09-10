#pragma once

#include "Parameter/ParameterModuleTypes.h"
#include "Event/EventModuleTypes.h"

#include "Event_StepCompleted.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FEventStepCompleted : public FEventBase
{
	GENERATED_BODY()

public:
	FEventStepCompleted() = default;

	FEventStepCompleted(class UStepBase* InStep)
		: Step(MoveTemp(InStep))
	{
	}

	UPROPERTY(BlueprintReadWrite)
	class UStepBase* Step = nullptr;
};

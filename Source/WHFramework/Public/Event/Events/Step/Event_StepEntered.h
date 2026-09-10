#pragma once

#include "Parameter/ParameterModuleTypes.h"
#include "Event/EventModuleTypes.h"

#include "Event_StepEntered.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FEventStepEntered : public FEventBase
{
	GENERATED_BODY()

public:
	FEventStepEntered() = default;

	FEventStepEntered(class UStepBase* InStep)
		: Step(MoveTemp(InStep))
	{
	}

	UPROPERTY(BlueprintReadWrite)
	class UStepBase* Step = nullptr;
};

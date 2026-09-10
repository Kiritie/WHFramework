#pragma once

#include "Event/EventModuleTypes.h"

#include "Event_StepExecuted.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FEventStepExecuted : public FEventBase
{
	GENERATED_BODY()

public:
	FEventStepExecuted() = default;

	FEventStepExecuted(class UStepBase* InStep)
		: Step(MoveTemp(InStep))
	{
	}

	UPROPERTY(BlueprintReadWrite)
	class UStepBase* Step = nullptr;
};

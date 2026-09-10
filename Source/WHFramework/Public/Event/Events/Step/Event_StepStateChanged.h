#pragma once

#include "Event/EventModuleTypes.h"

#include "Event_StepStateChanged.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FEventStepStateChanged : public FEventBase
{
	GENERATED_BODY()

public:
	FEventStepStateChanged() = default;

	FEventStepStateChanged(class UStepBase* InStep)
		: Step(MoveTemp(InStep))
	{
	}

	UPROPERTY(BlueprintReadWrite)
	class UStepBase* Step = nullptr;
};

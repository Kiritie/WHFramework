#pragma once

#include "Event/EventModuleTypes.h"

#include "Event_StepLeaved.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FEventStepLeaved : public FEventBase
{
	GENERATED_BODY()

public:
	FEventStepLeaved() = default;

	FEventStepLeaved(class UStepBase* InStep)
		: Step(MoveTemp(InStep))
	{
	}

	UPROPERTY(BlueprintReadWrite)
	class UStepBase* Step = nullptr;
};

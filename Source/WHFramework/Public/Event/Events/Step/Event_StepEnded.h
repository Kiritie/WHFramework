#pragma once

#include "Parameter/ParameterModuleTypes.h"
#include "Event/EventModuleTypes.h"

#include "Event_StepEnded.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FEventStepEnded : public FEventBase
{
	GENERATED_BODY()
};

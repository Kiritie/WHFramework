#pragma once

#include "Parameter/ParameterModuleTypes.h"
#include "Event/EventModuleTypes.h"

#include "Event_GameStarted.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FEventGameStarted : public FEventBase
{
	GENERATED_BODY()
};

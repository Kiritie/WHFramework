#pragma once

#include "Parameter/ParameterModuleTypes.h"
#include "Event/EventModuleTypes.h"

#include "Event_GameInited.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FEventGameInited : public FEventBase
{
	GENERATED_BODY()
};

#pragma once

#include "Parameter/ParameterModuleTypes.h"
#include "Event/EventModuleTypes.h"

#include "Event_GameExited.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FEventGameExited : public FEventBase
{
	GENERATED_BODY()

public:
	FEventGameExited() = default;

	FEventGameExited(bool InbIsSimulating)
		: bIsSimulating(MoveTemp(InbIsSimulating))
	{
	}

	UPROPERTY(BlueprintReadWrite)
	bool bIsSimulating = false;
};

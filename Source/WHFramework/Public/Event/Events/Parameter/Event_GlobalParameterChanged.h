#pragma once

#include "Parameter/ParameterModuleTypes.h"
#include "Event/EventModuleTypes.h"

#include "Event_GlobalParameterChanged.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FEventGlobalParameterChanged : public FEventBase
{
	GENERATED_BODY()

public:
	FEventGlobalParameterChanged() = default;

	FEventGlobalParameterChanged(FName InParameterName, FParameter InParameter)
		: ParameterName(MoveTemp(InParameterName)), Parameter(MoveTemp(InParameter))
	{
	}

	UPROPERTY(BlueprintReadWrite)
	FName ParameterName = NAME_None;

	UPROPERTY(BlueprintReadWrite)
	FParameter Parameter = FParameter();
};

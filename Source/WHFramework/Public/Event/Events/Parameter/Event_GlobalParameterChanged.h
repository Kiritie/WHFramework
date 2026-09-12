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

	FEventGlobalParameterChanged(FName InParamName, FParameter InParam)
		: ParameterName(MoveTemp(InParamName)), Parameter(MoveTemp(InParam))
	{
	}

	UPROPERTY(BlueprintReadWrite)
	FName ParameterName = NAME_None;

	UPROPERTY(BlueprintReadWrite)
	FParameter Parameter = FParameter();
};

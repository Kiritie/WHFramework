#pragma once

#include "Parameter/ParameterModuleTypes.h"
#include "Event/EventModuleTypes.h"

#include "Event_ProcedureEntered.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FEventProcedureEntered : public FEventBase
{
	GENERATED_BODY()

public:
	FEventProcedureEntered() = default;

	FEventProcedureEntered(class UProcedureBase* InProcedure)
		: Procedure(MoveTemp(InProcedure))
	{
	}

	UPROPERTY(BlueprintReadWrite)
	class UProcedureBase* Procedure = nullptr;
};

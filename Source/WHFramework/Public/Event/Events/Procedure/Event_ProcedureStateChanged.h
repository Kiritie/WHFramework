#pragma once

#include "Event/EventModuleTypes.h"

#include "Event_ProcedureStateChanged.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FEventProcedureStateChanged : public FEventBase
{
	GENERATED_BODY()

public:
	FEventProcedureStateChanged() = default;

	FEventProcedureStateChanged(class UProcedureBase* InProcedure)
		: Procedure(MoveTemp(InProcedure))
	{
	}

	UPROPERTY(BlueprintReadWrite)
	class UProcedureBase* Procedure = nullptr;
};

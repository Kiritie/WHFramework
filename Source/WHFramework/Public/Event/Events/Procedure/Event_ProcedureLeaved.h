#pragma once

#include "Event/EventModuleTypes.h"

#include "Event_ProcedureLeaved.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FEventProcedureLeaved : public FEventBase
{
	GENERATED_BODY()

public:
	FEventProcedureLeaved() = default;

	FEventProcedureLeaved(class UProcedureBase* InProcedure)
		: Procedure(MoveTemp(InProcedure))
	{
	}

	UPROPERTY(BlueprintReadWrite)
	class UProcedureBase* Procedure = nullptr;
};

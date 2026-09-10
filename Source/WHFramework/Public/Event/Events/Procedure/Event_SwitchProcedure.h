#pragma once

#include "Event/EventModuleTypes.h"

#include "Event_SwitchProcedure.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FEventSwitchProcedure : public FEventBase
{
	GENERATED_BODY()

public:
	FEventSwitchProcedure() = default;

	FEventSwitchProcedure(TSubclassOf<class UProcedureBase> InProcedureClass)
		: ProcedureClass(MoveTemp(InProcedureClass))
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<class UProcedureBase> ProcedureClass = nullptr;
};

#pragma once

#include "Parameter/ParameterModuleTypes.h"
#include "Event/EventModuleTypes.h"

#include "Event_FiniteStateEntered.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FEventFiniteStateEntered : public FEventBase
{
	GENERATED_BODY()

public:
	FEventFiniteStateEntered() = default;

	FEventFiniteStateEntered(class UFiniteStateBase* InState, class UFSMComponent* InFSM)
		: State(MoveTemp(InState)), FSM(MoveTemp(InFSM))
	{
	}

	UPROPERTY(BlueprintReadWrite)
	class UFiniteStateBase* State = nullptr;

	UPROPERTY(BlueprintReadWrite)
	class UFSMComponent* FSM = nullptr;
};

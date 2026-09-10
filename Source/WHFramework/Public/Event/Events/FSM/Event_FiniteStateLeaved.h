#pragma once

#include "Event/EventModuleTypes.h"

#include "Event_FiniteStateLeaved.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FEventFiniteStateLeaved : public FEventBase
{
	GENERATED_BODY()

public:
	FEventFiniteStateLeaved() = default;

	FEventFiniteStateLeaved(class UFiniteStateBase* InState, class UFSMComponent* InFSM)
		: State(MoveTemp(InState)), FSM(MoveTemp(InFSM))
	{
	}

	UPROPERTY(BlueprintReadWrite)
	class UFiniteStateBase* State = nullptr;

	UPROPERTY(BlueprintReadWrite)
	class UFSMComponent* FSM = nullptr;
};

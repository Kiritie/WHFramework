#pragma once

#include "Parameter/ParameterModuleTypes.h"
#include "Event/EventModuleTypes.h"

#include "Event_TaskEntered.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FEventTaskEntered : public FEventBase
{
	GENERATED_BODY()

public:
	FEventTaskEntered() = default;

	FEventTaskEntered(class UTaskBase* InTask)
		: Task(MoveTemp(InTask))
	{
	}

	UPROPERTY(BlueprintReadWrite)
	class UTaskBase* Task = nullptr;
};

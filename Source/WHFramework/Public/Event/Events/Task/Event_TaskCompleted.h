#pragma once

#include "Parameter/ParameterModuleTypes.h"
#include "Event/EventModuleTypes.h"

#include "Event_TaskCompleted.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FEventTaskCompleted : public FEventBase
{
	GENERATED_BODY()

public:
	FEventTaskCompleted() = default;

	FEventTaskCompleted(class UTaskBase* InTask)
		: Task(MoveTemp(InTask))
	{
	}

	UPROPERTY(BlueprintReadWrite)
	class UTaskBase* Task = nullptr;
};

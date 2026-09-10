#pragma once

#include "Event/EventModuleTypes.h"

#include "Event_TaskExecuted.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FEventTaskExecuted : public FEventBase
{
	GENERATED_BODY()

public:
	FEventTaskExecuted() = default;

	FEventTaskExecuted(class UTaskBase* InTask)
		: Task(MoveTemp(InTask))
	{
	}

	UPROPERTY(BlueprintReadWrite)
	class UTaskBase* Task = nullptr;
};

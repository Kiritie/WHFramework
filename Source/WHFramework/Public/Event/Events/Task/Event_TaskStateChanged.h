#pragma once

#include "Event/EventModuleTypes.h"

#include "Event_TaskStateChanged.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FEventTaskStateChanged : public FEventBase
{
	GENERATED_BODY()

public:
	FEventTaskStateChanged() = default;

	FEventTaskStateChanged(class UTaskBase* InTask)
		: Task(MoveTemp(InTask))
	{
	}

	UPROPERTY(BlueprintReadWrite)
	class UTaskBase* Task = nullptr;
};

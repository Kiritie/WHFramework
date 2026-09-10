#pragma once

#include "Event/EventModuleTypes.h"

#include "Event_TaskLeaved.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FEventTaskLeaved : public FEventBase
{
	GENERATED_BODY()

public:
	FEventTaskLeaved() = default;

	FEventTaskLeaved(class UTaskBase* InTask)
		: Task(MoveTemp(InTask))
	{
	}

	UPROPERTY(BlueprintReadWrite)
	class UTaskBase* Task = nullptr;
};

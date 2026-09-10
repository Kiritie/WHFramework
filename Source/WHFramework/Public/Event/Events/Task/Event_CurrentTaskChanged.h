#pragma once

#include "Parameter/ParameterModuleTypes.h"
#include "Event/EventModuleTypes.h"

#include "Event_CurrentTaskChanged.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FEventCurrentTaskChanged : public FEventBase
{
	GENERATED_BODY()

public:
	FEventCurrentTaskChanged() = default;

	FEventCurrentTaskChanged(class UTaskBase* InTask)
		: Task(MoveTemp(InTask))
	{
	}

	UPROPERTY(BlueprintReadWrite)
	class UTaskBase* Task = nullptr;
};

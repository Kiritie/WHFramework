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

	FEventCurrentTaskChanged(FGuid InPlayerId, class UTaskBase* InTask)
		: PlayerId(InPlayerId), Task(InTask)
	{
	}

	UPROPERTY(BlueprintReadWrite)
	FGuid PlayerId;

	UPROPERTY(BlueprintReadWrite)
	class UTaskBase* Task = nullptr;
};

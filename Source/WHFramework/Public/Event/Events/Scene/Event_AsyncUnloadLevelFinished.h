#pragma once

#include "Input/InputModuleTypes.h"
#include "Parameter/ParameterModuleTypes.h"
#include "Event/EventModuleTypes.h"

#include "Event_AsyncUnloadLevelFinished.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FEventAsyncUnloadLevelFinished : public FEventBase
{
	GENERATED_BODY()

public:
	FEventAsyncUnloadLevelFinished() = default;

	FEventAsyncUnloadLevelFinished(FName InLevelPath)
		: LevelPath(MoveTemp(InLevelPath))
	{
	}

	UPROPERTY(BlueprintReadWrite)
	FName LevelPath = NAME_None;
};

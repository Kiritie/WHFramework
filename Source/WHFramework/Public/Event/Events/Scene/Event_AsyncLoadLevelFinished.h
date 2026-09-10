#pragma once

#include "Input/InputModuleTypes.h"
#include "Parameter/ParameterModuleTypes.h"
#include "Event/EventModuleTypes.h"

#include "Event_AsyncLoadLevelFinished.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FEventAsyncLoadLevelFinished : public FEventBase
{
	GENERATED_BODY()

public:
	FEventAsyncLoadLevelFinished() = default;

	FEventAsyncLoadLevelFinished(FName InLevelPath)
		: LevelPath(MoveTemp(InLevelPath))
	{
	}

	UPROPERTY(BlueprintReadWrite)
	FName LevelPath = NAME_None;
};

#pragma once

#include "Common/CommonModuleTypes.h"
#include "Parameter/ParameterModuleTypes.h"
#include "Event/EventModuleTypes.h"

#include "Event_GamePaused.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FEventGamePaused : public FEventBase
{
	GENERATED_BODY()

public:
	FEventGamePaused() = default;

	FEventGamePaused(EPauseMode InPauseMode)
		: PauseMode(MoveTemp(InPauseMode))
	{
	}

	UPROPERTY(BlueprintReadWrite)
	EPauseMode PauseMode = EPauseMode::Default;
};

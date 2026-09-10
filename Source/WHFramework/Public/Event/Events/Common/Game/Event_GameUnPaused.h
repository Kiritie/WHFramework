#pragma once

#include "Common/CommonModuleTypes.h"
#include "Parameter/ParameterModuleTypes.h"
#include "Event/EventModuleTypes.h"

#include "Event_GameUnPaused.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FEventGameUnPaused : public FEventBase
{
	GENERATED_BODY()

public:
	FEventGameUnPaused() = default;

	FEventGameUnPaused(EPauseMode InPauseMode)
		: PauseMode(InPauseMode)
	{
	}

	UPROPERTY(BlueprintReadWrite)
	EPauseMode PauseMode = EPauseMode::Default;
};

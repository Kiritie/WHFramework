#pragma once

#include "Audio/AudioModuleTypes.h"
#include "Parameter/ParameterModuleTypes.h"
#include "Event/EventModuleTypes.h"

#include "Event_SoundParamsChanged.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FEventSoundParamsChanged : public FEventBase
{
	GENERATED_BODY()

public:
	FEventSoundParamsChanged() = default;

	FEventSoundParamsChanged(USoundClass* InSoundClass, FSoundParams InSoundParams)
		: SoundClass(MoveTemp(InSoundClass)), SoundParams(MoveTemp(InSoundParams))
	{
	}

	UPROPERTY(BlueprintReadWrite)
	USoundClass* SoundClass = nullptr;

	UPROPERTY(BlueprintReadWrite)
	FSoundParams SoundParams = FSoundParams();
};

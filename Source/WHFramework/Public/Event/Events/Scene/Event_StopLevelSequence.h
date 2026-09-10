#pragma once

#include "Parameter/ParameterModuleTypes.h"
#include "Event/EventModuleTypes.h"

#include "Event_StopLevelSequence.generated.h"

class ALevelSequenceActor;

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FEventStopLevelSequence : public FEventBase
{
	GENERATED_BODY()

public:
	FEventStopLevelSequence() = default;

	FEventStopLevelSequence(TSoftObjectPtr<ALevelSequenceActor> InLevelSequence, float InDelay, bool InbKeepState)
		: LevelSequence(MoveTemp(InLevelSequence)), Delay(MoveTemp(InDelay)), bKeepState(MoveTemp(InbKeepState))
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<ALevelSequenceActor> LevelSequence = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Delay = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bKeepState = false;
};

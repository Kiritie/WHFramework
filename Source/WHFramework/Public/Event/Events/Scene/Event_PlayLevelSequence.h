#pragma once

#include "Parameter/ParameterModuleTypes.h"
#include "Event/EventModuleTypes.h"

#include "Event_PlayLevelSequence.generated.h"

class ALevelSequenceActor;

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FEventPlayLevelSequence : public FEventBase
{
	GENERATED_BODY()

public:
	FEventPlayLevelSequence() = default;

	FEventPlayLevelSequence(TSoftObjectPtr<ALevelSequenceActor> InLevelSequence, float InDelay, bool InbReverse)
		: LevelSequence(MoveTemp(InLevelSequence)), Delay(MoveTemp(InDelay)), bReverse(MoveTemp(InbReverse))
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<ALevelSequenceActor> LevelSequence = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Delay = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bReverse = false;
};

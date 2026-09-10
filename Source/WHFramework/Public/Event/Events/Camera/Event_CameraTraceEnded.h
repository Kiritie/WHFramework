#pragma once

#include "Event/EventModuleTypes.h"

#include "Event_CameraTraceEnded.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FEventCameraTraceEnded : public FEventBase
{
	GENERATED_BODY()

public:
	FEventCameraTraceEnded() = default;

	FEventCameraTraceEnded(AActor* InTraceTarget)
		: TraceTarget(MoveTemp(InTraceTarget))
	{
	}

	UPROPERTY(BlueprintReadWrite)
	AActor* TraceTarget = nullptr;
};

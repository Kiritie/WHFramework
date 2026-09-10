#pragma once

#include "Event/EventModuleTypes.h"

#include "Event_SwitchCameraPoint.generated.h"

class ACameraPointBase;

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FEventSwitchCameraPoint : public FEventBase
{
	GENERATED_BODY()

public:
	FEventSwitchCameraPoint() = default;

	FEventSwitchCameraPoint(TSoftObjectPtr<ACameraPointBase> InCameraPoint)
		: CameraPoint(MoveTemp(InCameraPoint))
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<ACameraPointBase> CameraPoint = nullptr;
};

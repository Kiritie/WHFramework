#pragma once

#include "Event/EventModuleTypes.h"

#include "Event_CameraPointChanged.generated.h"

class ACameraPointBase;

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FEventCameraPointChanged : public FEventBase
{
	GENERATED_BODY()

public:
	FEventCameraPointChanged() = default;

	FEventCameraPointChanged(ACameraPointBase* InCameraPoint)
		: CameraPoint(MoveTemp(InCameraPoint))
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ACameraPointBase* CameraPoint = nullptr;
};

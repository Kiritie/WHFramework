#pragma once

#include "Camera/CameraModuleTypes.h"
#include "Event/EventModuleTypes.h"

#include "Event_ResetCameraView.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FEventResetCameraView : public FEventBase
{
	GENERATED_BODY()

public:
	FEventResetCameraView() = default;

	FEventResetCameraView(ECameraResetMode InCameraResetMode)
		: CameraResetMode(MoveTemp(InCameraResetMode))
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ECameraResetMode CameraResetMode = ECameraResetMode::DefaultPoint;
};

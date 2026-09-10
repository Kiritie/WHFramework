#pragma once

#include "Camera/CameraModuleTypes.h"
#include "Event/EventModuleTypes.h"

#include "Event_SetCameraView.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FEventSetCameraView : public FEventBase
{
	GENERATED_BODY()

public:
	FEventSetCameraView() = default;

	FEventSetCameraView(FCameraViewData InCameraViewData, bool InbCacheData)
		: CameraViewData(MoveTemp(InCameraViewData)), bCacheData(MoveTemp(InbCacheData))
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FCameraViewData CameraViewData = FCameraViewData();

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bCacheData = true;
};

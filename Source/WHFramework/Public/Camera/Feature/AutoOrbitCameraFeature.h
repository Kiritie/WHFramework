#pragma once
#include "Camera/Feature/CameraFeatureBase.h"
#include "AutoOrbitCameraFeature.generated.h"
UCLASS()
class WHFRAMEWORK_API UAutoOrbitCameraFeature : public UCameraFeatureBase
{
	GENERATED_BODY()
public:
	virtual void Apply(float DeltaTime, FCameraRigState& InOutState) override;
};

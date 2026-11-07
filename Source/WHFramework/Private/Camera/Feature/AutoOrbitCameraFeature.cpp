#include "Camera/Feature/AutoOrbitCameraFeature.h"
#include "Camera/CameraViewTypes.h"
void UAutoOrbitCameraFeature::Apply(float DeltaTime, FCameraRigState& InOutState)
{
	InOutState.PivotRotation.Yaw += Context.ScalarValue * DeltaTime;
}

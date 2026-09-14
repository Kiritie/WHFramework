#include "Camera/Feature/CameraFeatureBase.h"

void UCameraFeatureBase::Enter(ACameraManagerBase* InManager, const FCameraFeatureContext& InContext)
{
	CameraManager = InManager;
	Context = InContext;
}

void UCameraFeatureBase::Exit()
{
	CameraManager = nullptr;
}

void UCameraFeatureBase::Apply(float DeltaTime, FCameraRigState& InOutState)
{
}

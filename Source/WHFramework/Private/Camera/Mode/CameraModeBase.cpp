#include "Camera/Mode/CameraModeBase.h"
#include "Camera/CameraInputTypes.h"
#include "Camera/CameraViewTypes.h"
#include "Camera/CameraFeatureTypes.h"

void UCameraModeBase::Enter(ACameraManagerBase* InManager, const FCameraModeContext& Context)
{
	CameraManager = InManager;
}

void UCameraModeBase::Exit()
{
	CameraManager = nullptr;
}

void UCameraModeBase::Update(float DeltaTime, FCameraRigState& InOutState)
{
}

void UCameraModeBase::AddInput(const FCameraInputIntent& Intent)
{
}

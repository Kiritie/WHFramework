#include "Camera/Mode/CameraModeBase.h"

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

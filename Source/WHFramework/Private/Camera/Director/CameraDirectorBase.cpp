#include "Camera/Director/CameraDirectorBase.h"

void UCameraDirectorBase::Start(ACameraManagerBase* InManager)
{
	CameraManager = InManager;
}

void UCameraDirectorBase::Stop()
{
	CameraManager = nullptr;
}

void UCameraDirectorBase::Update(float DeltaTime)
{
}

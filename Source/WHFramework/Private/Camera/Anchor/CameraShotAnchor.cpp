#include "Camera/Anchor/CameraShotAnchor.h"
#include "Camera/CameraModule.h"

void ACameraShotAnchor::BeginPlay()
{
	Super::BeginPlay();
	if (UCameraModule* Cameras = UCameraModule::GetPtr(GetWorld())) Cameras->RegisterShotAnchor(this);
}

void ACameraShotAnchor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if(UCameraModule* Cameras = UCameraModule::GetPtr(GetWorld()))
	{
		Cameras->UnregisterShotAnchor(this);
	}
	Super::EndPlay(EndPlayReason);
}

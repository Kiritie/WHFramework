#include "Camera/Anchor/CameraShotAnchor.h"
#include "Camera/CameraModule.h"

void ACameraShotAnchor::BeginPlay()
{
	Super::BeginPlay();
	UCameraModule::Get().RegisterShotAnchor(this);
}

void ACameraShotAnchor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if(UCameraModule::IsValid())
	{
		UCameraModule::Get().UnregisterShotAnchor(this);
	}
	Super::EndPlay(EndPlayReason);
}

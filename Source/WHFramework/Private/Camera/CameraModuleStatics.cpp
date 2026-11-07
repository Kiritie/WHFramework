#include "Camera/CameraModuleStatics.h"

#include "Camera/CameraModule.h"
#include "Camera/Manager/CameraManagerBase.h"
#include "GameFramework/PlayerController.h"

ACameraManagerBase* UCameraModuleStatics::GetCameraManager(APlayerController* Player)
{
	UCameraModule* Module = Player ? UCameraModule::GetPtr(Player->GetWorld()) : nullptr;
	return Module ? Module->GetCameraManagerForPlayer(Player) : nullptr;
}

FCameraViewSnapshot UCameraModuleStatics::GetViewSnapshot(APlayerController* Player)
{
	const ACameraManagerBase* Manager = GetCameraManager(Player);
	return Manager ? Manager->GetFinalView() : FCameraViewSnapshot();
}

FTransform UCameraModuleStatics::GetViewTransform(APlayerController* Player)
{
	return GetViewSnapshot(Player).Transform;
}

FVector UCameraModuleStatics::GetViewLocation(APlayerController* Player)
{
	return GetViewTransform(Player).GetLocation();
}

FRotator UCameraModuleStatics::GetViewRotation(APlayerController* Player)
{
	return GetViewTransform(Player).Rotator();
}

float UCameraModuleStatics::GetViewFOV(APlayerController* Player)
{
	return GetViewSnapshot(Player).FOV;
}

void UCameraModuleStatics::SetCameraMode(APlayerController* Player, TSubclassOf<UCameraModeBase> Class, const FCameraModeContext& Context)
{
	if (ACameraManagerBase* Manager = GetCameraManager(Player)) Manager->SetMode(Class, Context);
}

FCameraFeatureHandle UCameraModuleStatics::PushFeature(APlayerController* Player, TSubclassOf<UCameraFeatureBase> Class, const FCameraFeatureContext& Context)
{
	if (ACameraManagerBase* Manager = GetCameraManager(Player)) return Manager->PushFeature(Class, Context);
	return {};
}

void UCameraModuleStatics::PopFeature(APlayerController* Player, FCameraFeatureHandle Handle)
{
	if (ACameraManagerBase* Manager = GetCameraManager(Player)) Manager->PopFeature(Handle);
}

void UCameraModuleStatics::StartDirector(APlayerController* Player, TSubclassOf<UCameraDirectorBase> Class)
{
	if (ACameraManagerBase* Manager = GetCameraManager(Player)) Manager->StartDirector(Class);
}

void UCameraModuleStatics::StopDirector(APlayerController* Player)
{
	if (ACameraManagerBase* Manager = GetCameraManager(Player)) Manager->StopDirector();
}

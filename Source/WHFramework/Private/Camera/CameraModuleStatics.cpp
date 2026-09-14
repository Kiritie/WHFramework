#include "Camera/CameraModuleStatics.h"
#include "Camera/CameraModule.h"
#include "Camera/Manager/CameraManagerBase.h"

ACameraManagerBase* UCameraModuleStatics::GetCameraManager(int32 Index)
{
	return UCameraModule::Get().GetCameraManager(Index);
}

FCameraViewSnapshot UCameraModuleStatics::GetViewSnapshot(int32 Index)
{
	const ACameraManagerBase* Manager = GetCameraManager(Index);
	return Manager ? Manager->GetFinalView() : FCameraViewSnapshot();
}

FTransform UCameraModuleStatics::GetViewTransform(int32 Index)
{
	return GetViewSnapshot(Index).Transform;
}

FVector UCameraModuleStatics::GetViewLocation(int32 Index)
{
	return GetViewTransform(Index).GetLocation();
}

FRotator UCameraModuleStatics::GetViewRotation(int32 Index)
{
	return GetViewTransform(Index).Rotator();
}

float UCameraModuleStatics::GetViewFOV(int32 Index)
{
	return GetViewSnapshot(Index).FOV;
}

void UCameraModuleStatics::SetCameraMode(TSubclassOf<UCameraModeBase> Class, const FCameraModeContext& Context, int32 Index)
{
	if(ACameraManagerBase* Manager = GetCameraManager(Index))
	{
		Manager->SetMode(Class, Context);
	}
}

FCameraFeatureHandle UCameraModuleStatics::PushFeature(TSubclassOf<UCameraFeatureBase> Class, const FCameraFeatureContext& Context, int32 Index)
{
	if(ACameraManagerBase* Manager = GetCameraManager(Index))
	{
		return Manager->PushFeature(Class, Context);
	}
	return {};
}

void UCameraModuleStatics::PopFeature(FCameraFeatureHandle Handle, int32 Index)
{
	if(ACameraManagerBase* Manager = GetCameraManager(Index))
	{
		Manager->PopFeature(Handle);
	}
}

void UCameraModuleStatics::StartDirector(TSubclassOf<UCameraDirectorBase> Class, int32 Index)
{
	if(ACameraManagerBase* Manager = GetCameraManager(Index))
	{
		Manager->StartDirector(Class);
	}
}

void UCameraModuleStatics::StopDirector(int32 Index)
{
	if(ACameraManagerBase* Manager = GetCameraManager(Index))
	{
		Manager->StopDirector();
	}
}

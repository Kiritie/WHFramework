// Fill out your copyright notice in the Description page of Project Settings.


#include "Camera/CameraModuleTypes.h"

#include "Camera/CameraModuleStatics.h"
#include "Camera/Actor/CameraActorBase.h"

void FCameraViewParams::GetCameraParams(AActor* InCameraViewTarget)
{
	const AActor* ViewTarget = InCameraViewTarget ? InCameraViewTarget : CameraViewTarget;
	if(CameraViewSpace == ECameraViewSpace::Local && ViewTarget)
	{
		CameraViewLocation = UCameraModuleStatics::GetCameraLocation() - ViewTarget->GetActorLocation();
		CameraViewYaw = UCameraModuleStatics::GetCameraRotation().Yaw - ViewTarget->GetActorRotation().Yaw;
		CameraViewPitch = UCameraModuleStatics::GetCameraRotation().Pitch - ViewTarget->GetActorRotation().Pitch;
	}
	else
	{
		CameraViewLocation = UCameraModuleStatics::GetCameraLocation();
		CameraViewYaw = UCameraModuleStatics::GetCameraRotation().Yaw;
		CameraViewPitch = UCameraModuleStatics::GetCameraRotation().Pitch;
	}
	CameraViewDistance = UCameraModuleStatics::GetCameraDistance();
}

void FCameraViewParams::SetCameraParams(const FCameraParams& InCameraParams, AActor* InCameraViewTarget)
{
	const AActor* ViewTarget = InCameraViewTarget ? InCameraViewTarget : CameraViewTarget;
	if(CameraViewSpace == ECameraViewSpace::Local && ViewTarget)
	{
		CameraViewLocation = InCameraParams.CameraLocation - ViewTarget->GetActorLocation();
		CameraViewYaw = InCameraParams.CameraRotation.Yaw - ViewTarget->GetActorRotation().Yaw;
		CameraViewPitch = InCameraParams.CameraRotation.Pitch - ViewTarget->GetActorRotation().Pitch;
	}
	else
	{
		CameraViewLocation = InCameraParams.CameraLocation;
		CameraViewYaw = InCameraParams.CameraRotation.Yaw;
		CameraViewPitch = InCameraParams.CameraRotation.Pitch;
	}
	CameraViewDistance = InCameraParams.CameraDistance;
}

FVector FCameraViewParams::GetCameraLocation(bool bApplyViewSpace) const
{
	if(bApplyViewSpace && CameraViewSpace == ECameraViewSpace::Local && CameraViewTarget)
	{
		return CameraViewTarget->GetActorLocation() + CameraViewLocation;
	}
	return CameraViewLocation;
}

FVector FCameraViewParams::GetCameraOffset() const
{
	return CameraViewOffset;
}

float FCameraViewParams::GetCameraYaw() const
{
	if(CameraViewSpace == ECameraViewSpace::Local && CameraViewTarget)
	{
		return CameraViewTarget->GetActorRotation().Yaw + CameraViewYaw;
	}
	return CameraViewYaw;
}

float FCameraViewParams::GetCameraPitch() const
{
	if(CameraViewSpace == ECameraViewSpace::Local && CameraViewTarget)
	{
		return CameraViewTarget->GetActorRotation().Pitch + CameraViewPitch;
	}
	return CameraViewPitch;
}

float FCameraViewParams::GetCameraDistance() const
{
	return CameraViewDistance;
}

float FCameraViewParams::GetCameraFov() const
{
	return CameraViewFov;
}

void FCameraViewData::FromParams(const TArray<FParameter>& InParams)
{
	if(InParams.Num() < 12) return;
	int32 i = 0;
	CameraViewTarget = InParams[i++];
	bTrackTarget = InParams[i++];
	TrackTargetMode = (ECameraTrackMode)InParams[i++].GetIntegerValue();
	CameraViewParams.CameraViewTarget = CameraViewTarget.LoadSynchronous();
	CameraViewParams.CameraViewActor = InParams[i++].GetObjectValue<ACameraActorBase>();
	CameraViewParams.CameraViewMode = (ECameraViewMode)InParams[i++].GetIntegerValue();
	CameraViewParams.CameraViewSpace = (ECameraViewSpace)InParams[i++].GetIntegerValue();
	CameraViewParams.CameraViewEaseType = (EEaseType)InParams[i++].GetIntegerValue();
	CameraViewParams.CameraViewDuration = InParams[i++];
	CameraViewParams.CameraViewLocation = InParams[i++];
	CameraViewParams.CameraViewYaw = InParams[i++];
	CameraViewParams.CameraViewPitch = InParams[i++];
	CameraViewParams.CameraViewDistance = InParams[i++];
	CameraViewParams.CameraViewFov = InParams[i++];
}

TArray<FParameter> FCameraViewData::ToParams() const
{
	return { CameraViewTarget, bTrackTarget, (int32)TrackTargetMode, CameraViewParams.CameraViewActor.LoadSynchronous(), (int32)CameraViewParams.CameraViewMode,
		(int32)CameraViewParams.CameraViewSpace, (int32)CameraViewParams.CameraViewEaseType, CameraViewParams.CameraViewDuration, CameraViewParams.CameraViewLocation,
		CameraViewParams.CameraViewYaw, CameraViewParams.CameraViewPitch, CameraViewParams.CameraViewDistance, CameraViewParams.CameraViewFov };
}

// Fill out your copyright notice in the Description page of Project Settings.


#include "Camera/CameraModuleTypes.h"

#include "Camera/CameraModuleStatics.h"
#include "Camera/Actor/CameraActorBase.h"

void FCameraViewParams::GetCameraParams()
{
	if(CameraViewSpace == ECameraViewSpace::Local && CameraViewTarget)
	{
		CameraViewOffset = UCameraModuleStatics::GetCameraLocation() - CameraViewTarget->GetActorLocation();
		CameraViewYaw = UCameraModuleStatics::GetCameraRotation().Yaw - CameraViewTarget->GetActorRotation().Yaw;
		CameraViewPitch = UCameraModuleStatics::GetCameraRotation().Pitch - CameraViewTarget->GetActorRotation().Pitch;
	}
	else
	{
		CameraViewOffset = UCameraModuleStatics::GetCameraLocation();
		CameraViewYaw = UCameraModuleStatics::GetCameraRotation().Yaw;
		CameraViewPitch = UCameraModuleStatics::GetCameraRotation().Pitch;
	}
	CameraViewDistance = UCameraModuleStatics::GetCameraDistance();
}

void FCameraViewParams::SetCameraParams(const FCameraParams& InCameraParams)
{
	if(CameraViewSpace == ECameraViewSpace::Local && CameraViewTarget)
	{
		CameraViewOffset = InCameraParams.CameraLocation - CameraViewTarget->GetActorLocation();
		CameraViewYaw = InCameraParams.CameraRotation.Yaw - CameraViewTarget->GetActorRotation().Yaw;
		CameraViewPitch = InCameraParams.CameraRotation.Pitch - CameraViewTarget->GetActorRotation().Pitch;
	}
	else
	{
		CameraViewOffset = InCameraParams.CameraLocation;
		CameraViewYaw = InCameraParams.CameraRotation.Yaw;
		CameraViewPitch = InCameraParams.CameraRotation.Pitch;
	}
	CameraViewDistance = InCameraParams.CameraDistance;
}

void FCameraViewData::FromParams(const TArray<FParameter>& InParams)
{
	int32 i = 0;
	CameraViewTarget = InParams[i++].GetObjectPtrValue<AActor>();
	bTrackTarget = InParams[i++].GetBooleanValue();
	TrackTargetMode = (ECameraTrackMode)InParams[i++].GetIntegerValue();
	CameraViewParams.CameraViewTarget = CameraViewTarget.Get();
	CameraViewParams.CameraViewActor = InParams[i++].GetObjectValue<ACameraActorBase>();
	CameraViewParams.CameraViewMode = (ECameraViewMode)InParams[i++].GetIntegerValue();
	CameraViewParams.CameraViewSpace = (ECameraViewSpace)InParams[i++].GetIntegerValue();
	CameraViewParams.CameraViewEaseType = (EEaseType)InParams[i++].GetIntegerValue();
	CameraViewParams.CameraViewDuration = InParams[i++].GetFloatValue();
	CameraViewParams.CameraViewOffset = InParams[i++].GetVectorValue();
	CameraViewParams.CameraViewYaw = InParams[i++].GetFloatValue();
	CameraViewParams.CameraViewPitch = InParams[i++].GetFloatValue();
	CameraViewParams.CameraViewDistance = InParams[i++].GetFloatValue();
}

TArray<FParameter> FCameraViewData::ToParams() const
{
	return { CameraViewTarget, bTrackTarget, (int32)TrackTargetMode, CameraViewParams.CameraViewActor.LoadSynchronous(), (int32)CameraViewParams.CameraViewMode,
		(int32)CameraViewParams.CameraViewSpace, (int32)CameraViewParams.CameraViewEaseType, CameraViewParams.CameraViewDuration, CameraViewParams.CameraViewOffset,
		CameraViewParams.CameraViewYaw, CameraViewParams.CameraViewPitch, CameraViewParams.CameraViewDistance };
}

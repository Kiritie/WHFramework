// Fill out your copyright notice in the Description page of Project Settings.

#include "Event/Handle/Camera/EventHandle_SwitchCameraPoint.h"
#include "Camera/Point/CameraPointBase.h"

UEventHandle_SwitchCameraPoint::UEventHandle_SwitchCameraPoint()
{
	CameraPoint = nullptr;
}

void UEventHandle_SwitchCameraPoint::OnDespawn_Implementation(bool bRecovery)
{
	CameraPoint = nullptr;
}

void UEventHandle_SwitchCameraPoint::Fill_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		CameraPoint = InParams[0].GetObjectValue<ACameraPointBase>();
	}
}

TArray<FParameter> UEventHandle_SwitchCameraPoint::Pack_Implementation() const
{
	return { CameraPoint };
}

// Fill out your copyright notice in the Description page of Project Settings.

#include "Event/Handle/Camera/EventHandle_CameraPointChanged.h"
#include "Camera/Point/CameraPointBase.h"

UEventHandle_CameraPointChanged::UEventHandle_CameraPointChanged()
{
	CameraPoint = nullptr;
}

void UEventHandle_CameraPointChanged::OnDespawn_Implementation(bool bRecovery)
{
	CameraPoint = nullptr;
}

void UEventHandle_CameraPointChanged::Parse_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		CameraPoint = InParams[0].GetObjectValue<ACameraPointBase>();
	}
}

TArray<FParameter> UEventHandle_CameraPointChanged::Pack_Implementation()
{
	return { CameraPoint };
}

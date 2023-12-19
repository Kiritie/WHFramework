// Fill out your copyright notice in the Description page of Project Settings.

#include "Event/Handle/Camera/EventHandle_SetCameraView.h"

UEventHandle_SetCameraView::UEventHandle_SetCameraView()
{
	CameraViewData = FCameraViewData();
}

void UEventHandle_SetCameraView::OnDespawn_Implementation(bool bRecovery)
{
	CameraViewData = FCameraViewData();
}

void UEventHandle_SetCameraView::Parse_Implementation(const TArray<FParameter>& InParams)
{
	CameraViewData.FromParams(InParams);
}

TArray<FParameter> UEventHandle_SetCameraView::Pack_Implementation()
{
	return CameraViewData.ToParams();
}

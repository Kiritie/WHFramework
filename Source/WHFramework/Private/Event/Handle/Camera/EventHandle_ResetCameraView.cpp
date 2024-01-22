// Fill out your copyright notice in the Description page of Project Settings.

#include "Event/Handle/Camera/EventHandle_ResetCameraView.h"

UEventHandle_ResetCameraView::UEventHandle_ResetCameraView()
{
	CameraResetMode = ECameraResetMode::DefaultPoint;
}

void UEventHandle_ResetCameraView::OnDespawn_Implementation(bool bRecovery)
{
	CameraResetMode = ECameraResetMode::DefaultPoint;
}

void UEventHandle_ResetCameraView::Parse_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		CameraResetMode = InParams[0].GetPointerValueRef<ECameraResetMode>();
	}
}

TArray<FParameter> UEventHandle_ResetCameraView::Pack_Implementation()
{
	return { &CameraResetMode };
}

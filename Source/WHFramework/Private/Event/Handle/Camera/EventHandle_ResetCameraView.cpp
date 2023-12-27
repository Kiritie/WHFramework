// Fill out your copyright notice in the Description page of Project Settings.

#include "Event/Handle/Camera/EventHandle_ResetCameraView.h"

UEventHandle_ResetCameraView::UEventHandle_ResetCameraView()
{
	bUseCachedData = false;
}

void UEventHandle_ResetCameraView::OnDespawn_Implementation(bool bRecovery)
{
	bUseCachedData = false;
}

void UEventHandle_ResetCameraView::Parse_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		bUseCachedData = InParams[0].GetBooleanValue();
	}
}

TArray<FParameter> UEventHandle_ResetCameraView::Pack_Implementation()
{
	return { bUseCachedData };
}

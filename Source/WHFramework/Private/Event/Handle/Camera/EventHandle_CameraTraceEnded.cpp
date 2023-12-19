// Fill out your copyright notice in the Description page of Project Settings.

#include "Event/Handle/Camera/EventHandle_CameraTraceEnded.h"

UEventHandle_CameraTraceEnded::UEventHandle_CameraTraceEnded()
{
	TraceTarget = nullptr;
}

void UEventHandle_CameraTraceEnded::OnDespawn_Implementation(bool bRecovery)
{
	TraceTarget = nullptr;
}

void UEventHandle_CameraTraceEnded::Parse_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		TraceTarget = InParams[0].GetObjectValue<AActor>();
	}
}

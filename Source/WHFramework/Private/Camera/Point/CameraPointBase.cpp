﻿// Fill out your copyright notice in the Description page of Project Settings.

#include "Camera/Point/CameraPointBase.h"

ACameraPointBase::ACameraPointBase()
{
	bCameraTrack = false;
	CameraTrackMode = ECameraTrackMode::LocationOnly;
	CameraViewParams = FCameraViewParams();
}

void ACameraPointBase::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();
}

void ACameraPointBase::OnPreparatory_Implementation(EPhase InPhase)
{
	Super::OnPreparatory_Implementation(InPhase);
}

void ACameraPointBase::OnRefresh_Implementation(float DeltaSeconds)
{
	Super::OnRefresh_Implementation(DeltaSeconds);
}

void ACameraPointBase::OnTermination_Implementation(EPhase InPhase)
{
	Super::OnTermination_Implementation(InPhase);
}

FCameraViewData ACameraPointBase::GetCameraViewData()
{
	return FCameraViewData(this, bCameraTrack, CameraTrackMode, CameraViewParams);
}

void ACameraPointBase::SetCameraViewData(const FCameraViewData& InCameraViewData)
{
	bCameraTrack = InCameraViewData.bTrackTarget;
	CameraTrackMode = InCameraViewData.TrackTargetMode;
	CameraViewParams = InCameraViewData.CameraViewParams;
}
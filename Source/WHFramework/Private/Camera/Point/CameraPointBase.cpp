// Fill out your copyright notice in the Description page of Project Settings.

#include "Camera/Point/CameraPointBase.h"

#include "Camera/CameraModuleStatics.h"

ACameraPointBase::ACameraPointBase()
{
	bCameraTrack = false;
	CameraTrackMode = ECameraTrackMode::LocationOnly;
	CameraViewParams = FCameraViewParams();

#if WITH_EDITORONLY_DATA
	bIsSpatiallyLoaded = false;
#endif
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

void ACameraPointBase::OnSwitch_Implementation()
{
	
}

void ACameraPointBase::Switch_Implementation(bool bCachePoint, bool bSetAsDefault)
{
	UCameraModuleStatics::SwitchCameraPoint(this, bCachePoint, bSetAsDefault);
}

#if WITH_EDITOR
void ACameraPointBase::GetCameraView()
{
	CameraViewParams.GetCameraParams(this);

	Modify();
}

void ACameraPointBase::SetCameraView(const FCameraParams& InCameraParams)
{
	CameraViewParams.SetCameraParams(InCameraParams, this);

	Modify();
}
#endif

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

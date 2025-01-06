// Fill out your copyright notice in the Description page of Project Settings.

#include "Camera/Point/CameraPointBase.h"

#include "Camera/CameraComponent.h"
#include "Camera/CameraModuleStatics.h"
#include "Components/WorldPartitionStreamingSourceComponent.h"
#include "GameFramework/SpringArmComponent.h"

ACameraPointBase::ACameraPointBase()
{
#if WITH_EDITORONLY_DATA
	bIsSpatiallyLoaded = false;
#endif
	
#if WITH_EDITORONLY_DATA
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(FName("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->SetRelativeTransform(FTransform::Identity);
	CameraBoom->bUsePawnControlRotation = false;

	Camera = CreateDefaultSubobject<UCameraComponent>(FName("Camera"));
	Camera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	Camera->SetRelativeLocationAndRotation(FVector(0, 0, 0), FRotator(0, 0, 0));
	Camera->bUsePawnControlRotation = false;
#endif

	StreamingSource = CreateDefaultSubobject<UWorldPartitionStreamingSourceComponent>(FName("StreamingSource"));
	StreamingSource->Priority = EStreamingSourcePriority::Default;
	StreamingSource->DisableStreamingSource();

	bCameraTrack = false;
	CameraTrackMode = ECameraTrackMode::LocationOnly;
	CameraViewParams = FCameraViewParams();
}

void ACameraPointBase::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();

	if(!bCameraTrack)
	{
		// StreamingSource->EnableStreamingSource();
	}
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

void ACameraPointBase::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

#if WITH_EDITORONLY_DATA
	const FVector CameraLocation = CameraViewParams.GetCameraLocation();
	const FVector CameraOffset = CameraViewParams.GetCameraOffset();
	const float CameraYaw = CameraViewParams.GetCameraYaw();
	const float CameraPitch = CameraViewParams.GetCameraPitch();
	const float CameraDistance = CameraViewParams.GetCameraDistance();
	const float CameraFov = CameraViewParams.GetCameraFov();

	// CameraBoom->SetRelativeLocation(CameraLocation);
	// CameraBoom->SetRelativeRotation(FRotator(CameraPitch, CameraYaw, 0.f));
	// CameraBoom->SocketOffset = CameraOffset;
	// CameraBoom->TargetArmLength = CameraDistance;
	//
	// Camera->SetFieldOfView(CameraFov != -1.f ? CameraFov : 90.f);
#endif
}

void ACameraPointBase::OnSwitch_Implementation()
{
	
}

void ACameraPointBase::Switch_Implementation(bool bSetAsDefault, bool bInstant)
{
	UCameraModuleStatics::SwitchCameraPoint(this, bSetAsDefault, bInstant);
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

bool ACameraPointBase::IsDefault() const
{
	return UCameraModuleStatics::GetDefaultCameraPoint() == this;
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

// Fill out your copyright notice in the Description page of Project Settings.


#include "Camera/Actor/CameraActorBase.h"

#include "Camera/CameraComponent.h"
#include "Camera/CameraModuleStatics.h"
#include "Components/WorldPartitionStreamingSourceComponent.h"
#include "GameFramework/SpringArmComponent.h"

// Sets default values
ACameraActorBase::ACameraActorBase()
{
#if WITH_EDITORONLY_DATA
	bIsSpatiallyLoaded = false;
#endif

	PrimaryActorTick.bCanEverTick = true;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(FName("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->SetRelativeTransform(FTransform::Identity);
	CameraBoom->bUsePawnControlRotation = false;

	Camera = CreateDefaultSubobject<UCameraComponent>(FName("Camera"));
	Camera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	Camera->SetRelativeLocationAndRotation(FVector(0, 0, 0), FRotator(0, 0, 0));
	Camera->bUsePawnControlRotation = false;
	
	StreamingSource = CreateDefaultSubobject<UWorldPartitionStreamingSourceComponent>(FName("StreamingSource"));
	StreamingSource->Priority = EStreamingSourcePriority::Highest;
	StreamingSource->DisableStreamingSource();

	CameraOrthoFactor = 1.93f;

	CameraCollisionMode = ECameraCollisionMode::None;
}

void ACameraActorBase::OnInitialize_Implementation()
{
	SetCameraCollisionMode(CameraCollisionMode);
}

void ACameraActorBase::OnPreparatory_Implementation(EPhase InPhase)
{
	
}

void ACameraActorBase::OnRefresh_Implementation(float DeltaSeconds)
{
	
}

void ACameraActorBase::OnTermination_Implementation(EPhase InPhase)
{
	
}

void ACameraActorBase::OnSwitch_Implementation()
{
	StreamingSource->EnableStreamingSource();
}

void ACameraActorBase::OnUnSwitch_Implementation()
{
	StreamingSource->DisableStreamingSource();
}

void ACameraActorBase::Switch_Implementation()
{
	UCameraModuleStatics::SwitchCamera(this);
}

void ACameraActorBase::UnSwitch_Implementation()
{
	if(IsCurrent())
	{
		UCameraModuleStatics::SwitchCamera(nullptr);
	}
}

bool ACameraActorBase::IsCurrent_Implementation() const
{
	return UCameraModuleStatics::GetCurrentCamera() == this;
}

void ACameraActorBase::SetCameraCollisionMode(ECameraCollisionMode InCameraCollisionMode)
{
	CameraCollisionMode = InCameraCollisionMode;
	CameraBoom->bDoCollisionTest = CameraCollisionMode == ECameraCollisionMode::All || CameraCollisionMode == ECameraCollisionMode::SightOnly;
}

void ACameraActorBase::SetCameraLocation(FVector InLocation)
{
	SetActorLocation(InLocation, CameraCollisionMode == ECameraCollisionMode::All || CameraCollisionMode == ECameraCollisionMode::PhysicsOnly);
}

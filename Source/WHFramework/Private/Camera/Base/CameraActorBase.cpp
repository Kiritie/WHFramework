// Fill out your copyright notice in the Description page of Project Settings.


#include "Camera/Base/CameraActorBase.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"

// Sets default values
ACameraActorBase::ACameraActorBase()
{
	PrimaryActorTick.bCanEverTick = true;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(FName("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->SetRelativeTransform(FTransform::Identity);
	CameraBoom->bUsePawnControlRotation = false;

	Camera = CreateDefaultSubobject<UCameraComponent>(FName("Camera"));
	Camera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	Camera->SetRelativeLocationAndRotation(FVector(0, 0, 0), FRotator(0, 0, 0));
	Camera->bUsePawnControlRotation = false;

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

void ACameraActorBase::SetCameraCollisionMode(ECameraCollisionMode InCameraCollisionMode)
{
	CameraCollisionMode = InCameraCollisionMode;
	CameraBoom->bDoCollisionTest = CameraCollisionMode == ECameraCollisionMode::All || CameraCollisionMode == ECameraCollisionMode::SightOnly;
}

void ACameraActorBase::SetCameraLocation(FVector InLocation)
{
	SetActorLocation(InLocation, CameraCollisionMode == ECameraCollisionMode::All || CameraCollisionMode == ECameraCollisionMode::PhysicsOnly);
}

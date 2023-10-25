// Fill out your copyright notice in the Description page of Project Settings.


#include "Camera/Roam/RoamCameraActor.h"

#include "Camera/CameraComponent.h"
#include "Camera/CameraModule.h"
#include "Components/SphereComponent.h"
#include "Engine/CollisionProfile.h"
#include "GameFramework/SpringArmComponent.h"

ARoamCameraActor::ARoamCameraActor()
{
	CameraName = FName("RoamCamera");

	Sphere = CreateDefaultSubobject<USphereComponent>(FName("Sphere"));
	Sphere->InitSphereRadius(35.0f);
	Sphere->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);
	RootComponent = Sphere;
	
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->SetRelativeTransform(FTransform::Identity);

	Camera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	Camera->SetRelativeLocationAndRotation(FVector(0, 0, 0), FRotator(0, 0, 0));
}

void ARoamCameraActor::MoveForward_Implementation(float InValue)
{
	const FVector Direction = GetActorRotation().Vector();
	if(ACameraModule* CameraModule = ACameraModule::Get())
	{
		CameraModule->AddCameraMovementInput(Direction, InValue);
	}
}

void ARoamCameraActor::MoveRight_Implementation(float InValue)
{
	const FVector Direction = FRotationMatrix(GetActorRotation()).GetUnitAxis(EAxis::Y);
	if(ACameraModule* CameraModule = ACameraModule::Get())
	{
		CameraModule->AddCameraMovementInput(Direction, InValue);
	}
}

void ARoamCameraActor::MoveUp_Implementation(float InValue)
{
	if(ACameraModule* CameraModule = ACameraModule::Get())
	{
		CameraModule->AddCameraMovementInput(FVector::UpVector, InValue);
	}
}

void ARoamCameraActor::SetCameraCollisionMode(ECameraCollisionMode InCameraCollisionMode)
{
	Super::SetCameraCollisionMode(InCameraCollisionMode);

	Sphere->SetCollisionEnabled(CameraCollisionMode == ECameraCollisionMode::All || CameraCollisionMode == ECameraCollisionMode::PhysicsOnly ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
}

// Fill out your copyright notice in the Description page of Project Settings.

#include "Camera/Actor/RoamCameraActor.h"

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
	Sphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RootComponent = Sphere;
	
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->SetRelativeTransform(FTransform::Identity);

	Camera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	Camera->SetRelativeLocationAndRotation(FVector(0, 0, 0), FRotator(0, 0, 0));

	bDrawDebug = false;
}

void ARoamCameraActor::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();
}

void ARoamCameraActor::SetCameraCollisionMode(ECameraCollisionMode InCameraCollisionMode)
{
	Super::SetCameraCollisionMode(InCameraCollisionMode);

	Sphere->SetCollisionEnabled(CameraCollisionMode == ECameraCollisionMode::All || CameraCollisionMode == ECameraCollisionMode::PhysicsOnly ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
}

// Fill out your copyright notice in the Description page of Project Settings.


#include "Camera/Roam/RoamCameraPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/CollisionProfile.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Gameplay/WHPlayerController.h"

ARoamCameraPawn::ARoamCameraPawn()
{
	CameraName = FName("RoamCamera");

	bUseControllerRotationPitch = true;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = true;

	Sphere = CreateDefaultSubobject<USphereComponent>(FName("Sphere"));
	Sphere->InitSphereRadius(35.0f);
	Sphere->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);
	RootComponent = Sphere;
	
	MovementComponent = CreateDefaultSubobject<UPawnMovementComponent, UFloatingPawnMovement>(FName("MovementComponent"));
	MovementComponent->UpdatedComponent = Sphere;
}

void ARoamCameraPawn::BeginPlay()
{
	Super::BeginPlay();

}

void ARoamCameraPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

// Called every frame
void ARoamCameraPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void ARoamCameraPawn::MoveForward(float InValue, bool b2DMode)
{
	FVector Direction;
	if(b2DMode)
	{
		Direction = FRotationMatrix(GetControlRotation()).GetUnitAxis(EAxis::Z);
	}
	else
	{
		Direction = GetControlRotation().Vector();
	}
	if(AWHPlayerController* PlayerController = GetController<AWHPlayerController>())
	{
		PlayerController->AddCameraMovementInput(Direction, InValue);
	}
}

void ARoamCameraPawn::MoveRight(float InValue, bool b2DMode)
{
	const FVector Direction = FRotationMatrix(GetControlRotation()).GetUnitAxis(EAxis::Y);
	if(AWHPlayerController* PlayerController = GetController<AWHPlayerController>())
	{
		PlayerController->AddCameraMovementInput(Direction, InValue);
	}
}

void ARoamCameraPawn::MoveUp(float InValue, bool b2DMode)
{
	if(AWHPlayerController* PlayerController = GetController<AWHPlayerController>())
	{
		PlayerController->AddCameraMovementInput(FVector::UpVector, InValue);
	}
}

void ARoamCameraPawn::SetCameraCollisionMode(ECameraCollisionMode InCameraCollisionMode)
{
	Super::SetCameraCollisionMode(InCameraCollisionMode);

	Sphere->SetCollisionEnabled(CameraCollisionMode == ECameraCollisionMode::All || CameraCollisionMode == ECameraCollisionMode::PhysicsOnly ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
}

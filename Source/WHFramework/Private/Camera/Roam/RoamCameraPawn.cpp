// Fill out your copyright notice in the Description page of Project Settings.


#include "Camera/Roam/RoamCameraPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/CollisionProfile.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Gameplay/WHPlayerController.h"

// Sets default values
ARoamCameraPawn::ARoamCameraPawn()
{
	CameraName = FName("RoamCamera");

	Sphere = CreateDefaultSubobject<USphereComponent>(FName("Sphere"));
	Sphere->InitSphereRadius(35.0f);
	Sphere->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);
	RootComponent = Sphere;
	
	MovementComponent = CreateDefaultSubobject<UPawnMovementComponent, UFloatingPawnMovement>(FName("MovementComponent"));
	MovementComponent->UpdatedComponent = Sphere;
}

// Called when the game starts or when spawned
void ARoamCameraPawn::BeginPlay()
{
	Super::BeginPlay();

}

// Called to bind functionality to input
void ARoamCameraPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAxis(FName("MoveForward"), this, &ARoamCameraPawn::MoveForward);
	PlayerInputComponent->BindAxis(FName("MoveRight"), this, &ARoamCameraPawn::MoveRight);
	PlayerInputComponent->BindAxis(FName("MoveUp"), this, &ARoamCameraPawn::MoveUp);
}

// Called every frame
void ARoamCameraPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void ARoamCameraPawn::MoveForward(float InValue)
{
	if(InValue == 0.f) return;
	
	if(AWHPlayerController* PlayerController = GetController<AWHPlayerController>())
	{
		PlayerController->AddCameraMovementInput(GetControlRotation().Vector(), InValue);
	}
}

void ARoamCameraPawn::MoveRight(float InValue)
{
	if(InValue == 0.f) return;
	
	const FRotator Rotation = GetControlRotation();
	const FRotator YawRotation(0, Rotation.Yaw, 0);
	const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
	if(AWHPlayerController* PlayerController = GetController<AWHPlayerController>())
	{
		PlayerController->AddCameraMovementInput(Direction, InValue);
	}
}

void ARoamCameraPawn::MoveUp(float InValue)
{
	if(InValue == 0.f) return;
	
	const FRotator Rotation = GetControlRotation();
	const FRotator YawRotation(0, Rotation.Yaw, 0);
	const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
	if(AWHPlayerController* PlayerController = GetController<AWHPlayerController>())
	{
		PlayerController->AddCameraMovementInput(FVector(Direction.X * 0.f, Direction.Y * 0.f, 1.f), InValue);
	}
}

void ARoamCameraPawn::SetCameraCollisionMode(ECameraCollisionMode InCameraCollisionMode)
{
	Super::SetCameraCollisionMode(InCameraCollisionMode);

	Sphere->SetCollisionEnabled(CameraCollisionMode == ECameraCollisionMode::All || CameraCollisionMode == ECameraCollisionMode::PhysicsOnly ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
}

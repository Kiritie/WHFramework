// Fill out your copyright notice in the Description page of Project Settings.


#include "Camera/Roam/RoamCameraPawn.h"

#include "Camera/CameraComponent.h"
#include "Camera/CameraModule.h"
#include "Components/SphereComponent.h"
#include "Engine/CollisionProfile.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Main/MainModule.h"

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
	
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->SetRelativeTransform(FTransform::Identity);

	Camera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	Camera->SetRelativeLocationAndRotation(FVector(0, 0, 0), FRotator(0, 0, 0));
	
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
	if(ACameraModule* CameraModule = AMainModule::GetModuleByClass<ACameraModule>())
	{
		CameraModule->AddCameraMovementInput(Direction, InValue);
	}
}

void ARoamCameraPawn::MoveRight(float InValue, bool b2DMode)
{
	const FVector Direction = FRotationMatrix(GetControlRotation()).GetUnitAxis(EAxis::Y);
	if(ACameraModule* CameraModule = AMainModule::GetModuleByClass<ACameraModule>())
	{
		CameraModule->AddCameraMovementInput(Direction, InValue);
	}
}

void ARoamCameraPawn::MoveUp(float InValue, bool b2DMode)
{
	if(ACameraModule* CameraModule = AMainModule::GetModuleByClass<ACameraModule>())
	{
		CameraModule->AddCameraMovementInput(FVector::UpVector, InValue);
	}
}

void ARoamCameraPawn::SetCameraCollisionMode(ECameraCollisionMode InCameraCollisionMode)
{
	Super::SetCameraCollisionMode(InCameraCollisionMode);

	Sphere->SetCollisionEnabled(CameraCollisionMode == ECameraCollisionMode::All || CameraCollisionMode == ECameraCollisionMode::PhysicsOnly ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
}

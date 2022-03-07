// Fill out your copyright notice in the Description page of Project Settings.


#include "Camera/Roam/RoamCameraPawn.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"

// Sets default values
ARoamCameraPawn::ARoamCameraPawn()
{
	CameraName = FName("RoamCamera");
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
	AddMovementInput(GetControlRotation().Vector(), InValue);
}

void ARoamCameraPawn::MoveRight(float InValue)
{
	const FRotator Rotation = GetControlRotation();
	const FRotator YawRotation(0, Rotation.Yaw, 0);
	const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
	AddMovementInput(Direction, InValue);
}

void ARoamCameraPawn::MoveUp(float InValue)
{
	const FRotator Rotation = GetControlRotation();
	const FRotator YawRotation(0, Rotation.Yaw, 0);
	const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
	AddMovementInput(FVector(Direction.X * 0.1f, Direction.Y * 0.1f, 1.f) * InValue, 0.5f);
}

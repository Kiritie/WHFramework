// Fill out your copyright notice in the Description page of Project Settings.


#include "Camera/Base/CameraPawnBase.h"

#include "Camera/CameraComponent.h"
#include "Debug/DebugModuleTypes.h"
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/KismetMathLibrary.h"

// Sets default values
ACameraPawnBase::ACameraPawnBase()
{
	PrimaryActorTick.bCanEverTick = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(FName("RootComponent"));

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(FName("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->SetRelativeTransform(FTransform::Identity);
	CameraBoom->bUsePawnControlRotation = true;

	Camera = CreateDefaultSubobject<UCameraComponent>(FName("Camera"));
	Camera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	Camera->SetRelativeLocationAndRotation(FVector(0, 0, 0), FRotator(0, 0, 0));
	Camera->bUsePawnControlRotation = false;

	MovementComponent = CreateDefaultSubobject<UPawnMovementComponent>(FName("MovementComponent"));
	//MovementComponent->UpdatedComponent = CameraBoom;

	CameraCollisionMode = ECameraCollisionMode::None;
}

void ACameraPawnBase::BeginPlay()
{
	Super::BeginPlay();

	SetCameraCollisionMode(CameraCollisionMode);
}

void ACameraPawnBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

void ACameraPawnBase::Tick(float DeltaTime) 
{
	Super::Tick(DeltaTime);
}

void ACameraPawnBase::SetCameraCollisionMode(ECameraCollisionMode InCameraCollisionMode)
{
	CameraCollisionMode = InCameraCollisionMode;
	if(GetCameraBoom())
	{
		GetCameraBoom()->bDoCollisionTest = CameraCollisionMode == ECameraCollisionMode::All || CameraCollisionMode == ECameraCollisionMode::SightOnly;
	}
}

// Fill out your copyright notice in the Description page of Project Settings.


#include "Camera/Base/CameraPawnBase.h"

#include "Camera/CameraComponent.h"
#include "Debug/DebugModuleTypes.h"
#include "GameFramework/SpringArmComponent.h"

// Sets default values
ACameraPawnBase::ACameraPawnBase()
{
 	// Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(FName("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 200; // The camera follows at this distance behind the character	
	CameraBoom->SocketOffset = FVector(0, 30, 40);
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(FName("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->SetRelativeLocationAndRotation(FVector(0, 0, 0), FRotator(0, 0, 0));
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	CameraTurnRate = 45;
	CameraLookUpRate = 45;

	MinCameraDistance = 100.f;
	MaxCameraDistance = 300.f;
	InitCameraDistance = 150.f;
	CameraZoomSpeed = 50.f;

	TargetCameraDistance = 0.f;
}

// Called when the game starts or when spawned
void ACameraPawnBase::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called to bind functionality to input
void ACameraPawnBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAxis(FName("Turn"), this, &ACameraPawnBase::TurnCam);
	PlayerInputComponent->BindAxis(FName("LookUp"), this, &ACameraPawnBase::LookUpCam);
	PlayerInputComponent->BindAxis(FName("ZoomCam"), this, &ACameraPawnBase::ZoomCam);
}

void ACameraPawnBase::Tick(float DeltaTime) 
{
	Super::Tick(DeltaTime);

	if (GetCameraBoom() && GetCameraBoom()->TargetArmLength != TargetCameraDistance)
	{
		GetCameraBoom()->TargetArmLength = FMath::FInterpTo(GetCameraBoom()->TargetArmLength, TargetCameraDistance, DeltaTime, CameraZoomSpeed);
	}
}
void ACameraPawnBase::TurnCam(float InRate)
{
	AddControllerYawInput(InRate * CameraTurnRate * GetWorld()->GetDeltaSeconds());
}

void ACameraPawnBase::LookUpCam(float InRate)
{
	AddControllerPitchInput(InRate * CameraLookUpRate * GetWorld()->GetDeltaSeconds());
}

void ACameraPawnBase::ZoomCam(float InRate)
{
	if (InRate != 0.f && GetCameraBoom())
	{
		SetCameraDistance(TargetCameraDistance + InRate * 100.f);
	}
}

void ACameraPawnBase::SetCameraDistance(float InCameraDistance, bool bInstant)
{
	TargetCameraDistance = InCameraDistance != -1.f ? FMath::Clamp(InCameraDistance, MinCameraDistance, MaxCameraDistance) : InitCameraDistance;
	if(bInstant && GetCameraBoom()) GetCameraBoom()->TargetArmLength = TargetCameraDistance;
}

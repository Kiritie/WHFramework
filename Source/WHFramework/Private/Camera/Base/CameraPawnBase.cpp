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

void ACameraPawnBase::OnInitialize_Implementation()
{
	SetCameraCollisionMode(CameraCollisionMode);
}

void ACameraPawnBase::OnPreparatory_Implementation(EPhase InPhase)
{
	
}

void ACameraPawnBase::OnRefresh_Implementation(float DeltaSeconds)
{
	
}

void ACameraPawnBase::OnTermination_Implementation(EPhase InPhase)
{
	
}

void ACameraPawnBase::BeginPlay()
{
	Super::BeginPlay();

	if(Execute_IsDefaultLifecycle(this))
	{
		Execute_OnInitialize(this);
		Execute_OnPreparatory(this, EPhase::Final);
	}
}

void ACameraPawnBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	if(Execute_IsDefaultLifecycle(this))
	{
		Execute_OnTermination(this, EPhase::Final);
	}
}

void ACameraPawnBase::Tick(float DeltaSeconds) 
{
	Super::Tick(DeltaSeconds);

	if(Execute_IsDefaultLifecycle(this))
	{
		Execute_OnRefresh(this, DeltaSeconds);
	}
}	

void ACameraPawnBase::SetCameraCollisionMode(ECameraCollisionMode InCameraCollisionMode)
{
	CameraCollisionMode = InCameraCollisionMode;
	if(Execute_GetCameraBoom(this))
	{
		Execute_GetCameraBoom(this)->bDoCollisionTest = CameraCollisionMode == ECameraCollisionMode::All || CameraCollisionMode == ECameraCollisionMode::SightOnly;
	}
}

bool ACameraPawnBase::GenerateVoxel(const FVoxelHitResult& InVoxelHitResult)
{
	return IVoxelAgentInterface::GenerateVoxel(InVoxelHitResult);
}

bool ACameraPawnBase::DestroyVoxel(const FVoxelHitResult& InVoxelHitResult)
{
	return IVoxelAgentInterface::DestroyVoxel(InVoxelHitResult);
}

// Fill out your copyright notice in the Description page of Project Settings.


#include "Camera/Actor/CameraActorBase.h"

#include "Camera/CameraComponent.h"
#include "Camera/CameraModuleStatics.h"
#include "Components/WorldPartitionStreamingSourceComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Scene/SceneModuleStatics.h"

// Sets default values
ACameraActorBase::ACameraActorBase()
{
#if WITH_EDITORONLY_DATA
	bIsSpatiallyLoaded = false;
#endif

	PrimaryActorTick.bCanEverTick = true;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(FName("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->SetRelativeTransform(FTransform::Identity);
	CameraBoom->bUsePawnControlRotation = false;
	CameraBoom->bDoCollisionTest = false;

	Camera = CreateDefaultSubobject<UCameraComponent>(FName("Camera"));
	Camera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	Camera->SetRelativeLocationAndRotation(FVector(0, 0, 0), FRotator(0, 0, 0));
	Camera->bUsePawnControlRotation = false;
	
	StreamingSource = CreateDefaultSubobject<UWorldPartitionStreamingSourceComponent>(FName("StreamingSource"));
	StreamingSource->Priority = EStreamingSourcePriority::Highest;
	StreamingSource->DisableStreamingSource();

	CameraOrthoFactor = 1.93f;
	CameraStreamingAltitude = -1.f;
	CameraStreamingVelocity = -1.f;

	LastCameraLocation = FVector::ZeroVector;
	CameraStayDuration = 0.f;

	CameraCollisionMode = ECameraCollisionMode::None;
}

void ACameraActorBase::OnInitialize_Implementation()
{
	AWHActor::OnInitialize_Implementation();
	
	IDebuggerInterface::Register();

	SetCameraCollisionMode(CameraCollisionMode);
}

void ACameraActorBase::OnPreparatory_Implementation(EPhase InPhase)
{
	AWHActor::OnPreparatory_Implementation(InPhase);
	
	LastCameraLocation = GetActorLocation();
}

void ACameraActorBase::OnRefresh_Implementation(float DeltaSeconds)
{
	AWHActor::OnRefresh_Implementation(DeltaSeconds);

	CameraVelocity = GetActorLocation() - LastCameraLocation;

	if(!GetActorLocation().Equals(LastCameraLocation, 0.1f))
	{
		CameraStayDuration = 0.f;
	}
	else
	{
		CameraStayDuration += DeltaSeconds;
	}
	
	if(IsCurrent())
	{
		if((CameraStreamingAltitude == -1.f || USceneModuleStatics::GetAltitude() < CameraStreamingAltitude)
			&& (CameraStreamingVelocity == -1.f || (CameraVelocity.Length() < CameraStreamingVelocity && CameraStayDuration >= 1.f)))
		{
			StreamingSource->EnableStreamingSource();
		}
		else
		{
			StreamingSource->DisableStreamingSource();
		}
	}

	LastCameraLocation = GetActorLocation();
}

void ACameraActorBase::OnTermination_Implementation(EPhase InPhase)
{
	AWHActor::OnTermination_Implementation(InPhase);

	IDebuggerInterface::UnRegister();
}

void ACameraActorBase::OnDrawDebug(UCanvas* InCanvas, APlayerController* InPC)
{
	DrawDebugCanvasWireSphere(InCanvas, GetActorLocation(), FColor::Red, GetCameraDistance() * 0.01f, 100);

	DrawDebugCanvasLine(InCanvas, GetActorLocation(), Camera->GetComponentLocation(), FLinearColor::Red);
}

void ACameraActorBase::OnSwitch_Implementation()
{
	
}

void ACameraActorBase::OnUnSwitch_Implementation()
{
	StreamingSource->DisableStreamingSource();
}

void ACameraActorBase::Switch_Implementation()
{
	UCameraModuleStatics::SwitchCamera(this);
}

void ACameraActorBase::UnSwitch_Implementation()
{
	if(IsCurrent())
	{
		UCameraModuleStatics::SwitchCamera(nullptr);
	}
}

bool ACameraActorBase::IsCurrent_Implementation() const
{
	return UCameraModuleStatics::GetCurrentCamera() == this;
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

float ACameraActorBase::GetCameraDistance() const
{
	return CameraBoom->TargetArmLength;
}

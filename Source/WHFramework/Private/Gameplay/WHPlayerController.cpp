// Fill out your copyright notice in the Description page of Project Settings.

#include "Gameplay/WHPlayerController.h"

#include "Net/UnrealNetwork.h"
#include "Audio/AudioModuleNetworkComponent.h"
#include "Camera/CameraModuleNetworkComponent.h"
#include "Character/CharacterModuleNetworkComponent.h"
#include "Components/WidgetInteractionComponent.h"
#include "Event/EventModuleNetworkComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Media/MediaModuleNetworkComponent.h"
#include "Network/NetworkModuleNetworkComponent.h"
#include "Procedure/ProcedureModule.h"
#include "Procedure/ProcedureModuleNetworkComponent.h"

AWHPlayerController::AWHPlayerController()
{
	WidgetInteractionComp = CreateDefaultSubobject<UWidgetInteractionComponent>(FName("WidgetInteractionComp"));
	WidgetInteractionComp->SetupAttachment(RootComponent);

	ModuleNetCompMap = TMap<TSubclassOf<UModuleNetworkComponent>, UModuleNetworkComponent*>();
	
	AudioModuleNetComp = CreateDefaultSubobject<UAudioModuleNetworkComponent>(FName("AudioModuleNetComp"));
	ModuleNetCompMap.Add(UAudioModuleNetworkComponent::StaticClass(), AudioModuleNetComp);
	
	CameraModuleNetComp = CreateDefaultSubobject<UCameraModuleNetworkComponent>(FName("CameraModuleNetComp"));
	ModuleNetCompMap.Add(UCameraModuleNetworkComponent::StaticClass(), CameraModuleNetComp);
	
	CharacterModuleNetComp = CreateDefaultSubobject<UCharacterModuleNetworkComponent>(FName("CharacterModuleNetComp"));
	ModuleNetCompMap.Add(UCharacterModuleNetworkComponent::StaticClass(), CharacterModuleNetComp);
	
	EventModuleNetComp = CreateDefaultSubobject<UEventModuleNetworkComponent>(FName("EventModuleNetComp"));
	ModuleNetCompMap.Add(UEventModuleNetworkComponent::StaticClass(), EventModuleNetComp);
	
	MediaModuleNetComp = CreateDefaultSubobject<UMediaModuleNetworkComponent>(FName("MediaModuleNetComp"));
	ModuleNetCompMap.Add(UMediaModuleNetworkComponent::StaticClass(), MediaModuleNetComp);
	
	NetworkModuleNetComp = CreateDefaultSubobject<UNetworkModuleNetworkComponent>(FName("NetworkModuleNetComp"));
	ModuleNetCompMap.Add(UNetworkModuleNetworkComponent::StaticClass(), NetworkModuleNetComp);
	
	ProcedureModuleNetComp = CreateDefaultSubobject<UProcedureModuleNetworkComponent>(FName("ProcedureModuleNetComp"));
	ModuleNetCompMap.Add(UProcedureModuleNetworkComponent::StaticClass(), ProcedureModuleNetComp);
	
	bCameraControlAble = false;
	bCameraMoveAble = true;
	bCameraRotateAble = true;
	bCameraZoomAble = true;

	bNormalizeMove = false;
	bNormalizeRotate = false;
	bNormalizeZoom = false;

	CameraMoveRate = 20.f;
	CameraMoveSmooth = 5.f;

	CameraTurnRate = 60.f;
	CameraLookUpRate = 60.f;
	CameraRotateSmooth = 2.f;
	MinCameraPinch = -89.f;
	MaxCameraPinch = 89.f;
	InitCameraPinch = -10.f;

	CameraZoomRate = 150.f;
	CameraZoomSmooth = 5.f;
	MinCameraDistance = 100.f;
	MaxCameraDistance = 300.f;
	InitCameraDistance = 150.f;

	TrackTargetActor = nullptr;
	TrackLocationOffset = FVector::ZeroVector;
	TrackYawOffset = 0.f;
	TrackPitchOffset = 0.f;
	TrackDistance = 0.f;
	TrackTargetMode = ETrackTargetMode::LocationOnly;
	
	TargetCameraLocation = FVector::ZeroVector;
	TargetCameraRotation = FRotator::ZeroRotator;
	TargetCameraDistance = 0.f;
}

void AWHPlayerController::Initialize()
{
	
}

void AWHPlayerController::BeginPlay()
{
	Super::BeginPlay();
}

void AWHPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	
	InputComponent->BindAxis(FName("Turn"), this, &AWHPlayerController::Turn);
	InputComponent->BindAxis(FName("LookUp"), this, &AWHPlayerController::LookUp);
	InputComponent->BindAxis(FName("PanH"), this, &AWHPlayerController::PanH);
	InputComponent->BindAxis(FName("PanV"), this, &AWHPlayerController::PanV);
	InputComponent->BindAxis(FName("Pinch"), this, &AWHPlayerController::Pinch);
	InputComponent->BindAxis(FName("ZoomCam"), this, &AWHPlayerController::ZoomCam);

	InputComponent->BindAction(FName("Interact"), EInputEvent::IE_Pressed, this, &AWHPlayerController::StartInteract);
	InputComponent->BindAction(FName("Interact"), EInputEvent::IE_Released, this, &AWHPlayerController::EndInteract);

	InputComponent->BindTouch(EInputEvent::IE_Pressed, this, &AWHPlayerController::TouchPressed);
	InputComponent->BindTouch(EInputEvent::IE_Released, this, &AWHPlayerController::TouchReleased);
	InputComponent->BindTouch(EInputEvent::IE_Axis, this, &AWHPlayerController::TouchMoved);
}

void AWHPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	SetCameraLocation(InPawn->GetActorLocation(), true);
}

void AWHPlayerController::OnUnPossess()
{
	Super::OnUnPossess();
}

void AWHPlayerController::Tick(float DeltaTime) 
{
	Super::Tick(DeltaTime);

	if(!bCameraControlAble) return;

	TrackTarget();

	if(GetPawn() && !GetPawn()->GetActorLocation().Equals(TargetCameraLocation))
	{
		GetPawn()->SetActorLocation(CameraMoveSmooth == 0 ? TargetCameraLocation : FMath::VInterpTo(GetPawn()->GetActorLocation(), TargetCameraLocation, DeltaTime, CameraMoveSmooth));
	}

	if(!GetControlRotation().Equals(TargetCameraRotation))
	{
		SetControlRotation(CameraRotateSmooth == 0 ? TargetCameraRotation : FMath::RInterpTo(GetControlRotation(), TargetCameraRotation, DeltaTime, CameraRotateSmooth));
	}

	if(GetCameraBoom() && GetCameraBoom()->TargetArmLength != TargetCameraDistance)
	{
		GetCameraBoom()->TargetArmLength = CameraZoomSmooth == 0 ? TargetCameraDistance : FMath::FInterpTo(GetCameraBoom()->TargetArmLength, TargetCameraDistance, DeltaTime, UKismetMathLibrary::NormalizeToRange(GetCameraBoom()->TargetArmLength, MinCameraDistance, MaxCameraDistance) * CameraZoomSmooth);
	}
}
void AWHPlayerController::Turn(float InRate)
{
	if(InRate == 0.f || !bCameraRotateAble) return;

	if(IsInputKeyDown(FKey(TEXT("RightMouseButton"))))
	{
		AddCameraRotationInput(InRate, 0.f);
	}
}

void AWHPlayerController::LookUp(float InRate)
{
	if(InRate == 0.f || !bCameraRotateAble) return;

	if(IsInputKeyDown(FKey(TEXT("RightMouseButton"))))
	{
		AddCameraRotationInput(0.f, -InRate);
	}
}

void AWHPlayerController::PanH(float InRate)
{
	if(InRate == 0.f || !bCameraMoveAble) return;

	if(IsInputKeyDown(FKey(TEXT("MiddleMouseButton"))))
	{
		const FRotator Rotation = GetControlRotation();
		//const FRotator YawRotation(0, Rotation.Yaw, 0);
		const FVector Direction = FRotationMatrix(Rotation).GetUnitAxis(EAxis::Y) * -1.f;
		AddCameraMovementInput(Direction, InRate);
	}
}

void AWHPlayerController::PanV(float InRate)
{
	if(InRate == 0.f || !bCameraMoveAble) return;

	if(IsInputKeyDown(FKey(TEXT("MiddleMouseButton"))))
	{
		const FRotator Rotation = GetControlRotation();
		const FVector Direction = FRotationMatrix(Rotation).GetUnitAxis(EAxis::Z) * -1.f;;
		AddCameraMovementInput(Direction, InRate);
	}
}

void AWHPlayerController::Pinch(float InRate)
{
	if(InRate == 0.f || !bCameraZoomAble) return;
	
	if(TouchPressedCount == 2)
	{
		float PinchDeltaValue;
		if(InRate > TouchPinchValuePrevious)
		{
			PinchDeltaValue = -(InRate / TouchPinchValuePrevious - 1.f);
		}
		else
		{
			PinchDeltaValue = TouchPinchValuePrevious / InRate - 1.f;
		}
		if(FMath::Abs(PinchDeltaValue) >= 1.f)
		{
			TouchPinchValuePrevious = InRate;
		}
		else
		{
			AddCameraDistanceInput(PinchDeltaValue);
		}
	}
}

void AWHPlayerController::ZoomCam(float InRate)
{
	if(InRate == 0.f || !bCameraZoomAble) return;

	AddCameraDistanceInput(InRate);
}

void AWHPlayerController::TouchPressed(ETouchIndex::Type InTouchIndex, FVector InLocation)
{
	TouchPressedCount++;
}

void AWHPlayerController::TouchReleased(ETouchIndex::Type InTouchIndex, FVector InLocation)
{
	TouchPressedCount--;

	float TouchLocationX;
	float TouchLocationY;
	bool bIsCurrentPressed;
	GetInputTouchState(ETouchIndex::Touch1, TouchLocationX, TouchLocationY, bIsCurrentPressed);
	
	TouchLocationPrevious = FVector2D(TouchLocationX, TouchLocationY);
	TouchPinchValuePrevious = GetInputAxisValue(FName("Pinch"));
}

void AWHPlayerController::TouchMoved(ETouchIndex::Type InTouchIndex, FVector InLocation)
{
	float TouchLocationX;
	float TouchLocationY;
	bool bIsCurrentPressed;
	GetInputTouchState(ETouchIndex::Touch1, TouchLocationX, TouchLocationY, bIsCurrentPressed);
	
	if(TouchPressedCount == 1)
	{
		if(bIsCurrentPressed)
		{
			AddCameraRotationInput(TouchLocationX / TouchLocationPrevious.X - 1.f, -(TouchLocationY / TouchLocationPrevious.Y - 1.f));
			TouchLocationPrevious = FVector2D(TouchLocationX, TouchLocationY);
		}
	}
	else if(TouchPressedCount >= 3)
	{
		const FRotator Rotation = GetControlRotation();
		const FVector DirectionH = FRotationMatrix(Rotation).GetUnitAxis(EAxis::Y) * (TouchLocationX / TouchLocationPrevious.X - 1.f);
		const FVector DirectionV = FRotationMatrix(Rotation).GetUnitAxis(EAxis::Z) * -(TouchLocationY / TouchLocationPrevious.Y - 1.f);
		AddCameraMovementInput(DirectionH + DirectionV, 1.f);
	}
}

void AWHPlayerController::StartInteract(FKey InKey)
{
	WidgetInteractionComp->PressPointerKey(InKey);
}

void AWHPlayerController::EndInteract(FKey InKey)
{
	WidgetInteractionComp->ReleasePointerKey(InKey);
}

void AWHPlayerController::TrackTarget(bool bInstant)
{
	if(!TrackTargetActor) return;
	
	switch(TrackTargetMode)
	{
		case ETrackTargetMode::LocationOnly:
		{
			SetCameraLocation(TrackTargetActor->GetActorLocation() + TrackTargetActor->GetActorRotation().RotateVector(TrackLocationOffset), bInstant);
			break;
		}
		case ETrackTargetMode::LocationAndRotation:
		{
			SetCameraLocation(TrackTargetActor->GetActorLocation() + TrackTargetActor->GetActorRotation().RotateVector(TrackLocationOffset), bInstant);
			SetCameraRotation(TrackTargetActor->GetActorRotation().Yaw + TrackYawOffset, TrackTargetActor->GetActorRotation().Pitch + TrackPitchOffset, bInstant);
			break;
		}
		case ETrackTargetMode::LocationAndRotationAndDistance:
		{
			SetCameraLocation(TrackTargetActor->GetActorLocation() + TrackTargetActor->GetActorRotation().RotateVector(TrackLocationOffset), bInstant);
			SetCameraRotation(TrackTargetActor->GetActorRotation().Yaw + TrackYawOffset, TrackTargetActor->GetActorRotation().Pitch + TrackPitchOffset, bInstant);
			SetCameraDistance(TrackDistance, bInstant);
			break;
		}
		case ETrackTargetMode::LocationAndRotationAndDistanceOnce:
		{
			SetCameraLocation(TrackTargetActor->GetActorLocation() + TrackTargetActor->GetActorRotation().RotateVector(TrackLocationOffset), bInstant);
			SetCameraRotation(TrackTargetActor->GetActorRotation().Yaw + TrackYawOffset, TrackTargetActor->GetActorRotation().Pitch + TrackPitchOffset, bInstant);
			SetCameraDistance(TrackDistance, bInstant);
			TrackTargetMode = ETrackTargetMode::LocationAndRotation;
			break;
		}
	}
}

void AWHPlayerController::StartTrackTarget(AActor* InTargetActor, ETrackTargetMode InTrackTargetMode, FVector InLocationOffset, float InYawOffset, float InPitchOffset, float InDistance, bool bInstant)
{
	if(!InTargetActor) return;

	if(TrackTargetActor != InTargetActor)
	{
		TrackTargetActor = InTargetActor;
		TrackTargetMode = InTrackTargetMode;
		TrackLocationOffset = InLocationOffset;
		TrackYawOffset = InYawOffset;
		TrackPitchOffset = InPitchOffset;
		TrackDistance = InDistance;
		TrackTarget(bInstant);
	}
}

void AWHPlayerController::EndTrackTarget()
{
	TrackTargetActor = nullptr;
}

void AWHPlayerController::SetCameraLocation(FVector InLocation, bool bInstant)
{
	TargetCameraLocation = InLocation;
	if(GetPawn() && bInstant)
	{
		GetPawn()->SetActorLocation(TargetCameraLocation);
	}
}

void AWHPlayerController::SetCameraRotation(float InYaw, float InPitch, bool bInstant)
{
	const FRotator CurrentRot = GetControlRotation();
	TargetCameraRotation = FRotator(FMath::Clamp(InPitch == -1.f ? InitCameraPinch : InPitch, MinCameraPinch, MaxCameraPinch), InYaw == -1.f ? CurrentRot.Yaw : InYaw, CurrentRot.Roll);
	if(bInstant)
	{
		SetControlRotation(TargetCameraRotation);
	}
}

void AWHPlayerController::SetCameraDistance(float InDistance, bool bInstant)
{
	TargetCameraDistance = InDistance != -1.f ? FMath::Clamp(InDistance, MinCameraDistance, MaxCameraDistance) : InitCameraDistance;
	if(GetCameraBoom() && bInstant)
	{
		GetCameraBoom()->TargetArmLength = TargetCameraDistance;
	}
}

void AWHPlayerController::SetCameraRotationAndDistance(float InYaw, float InPitch, float InDistance, bool bInstant)
{
	SetCameraRotation(InYaw, InPitch, bInstant);
	SetCameraDistance(InDistance, bInstant);
}

void AWHPlayerController::AddCameraRotationInput(float InYaw, float InPitch)
{
	if(!bCameraRotateAble) return;

	SetCameraRotation(TargetCameraRotation.Yaw + InYaw * CameraTurnRate * GetWorld()->GetDeltaSeconds(), TargetCameraRotation.Pitch + InPitch * CameraLookUpRate * GetWorld()->GetDeltaSeconds(), false);
}

void AWHPlayerController::AddCameraDistanceInput(float InValue)
{
	if(!bCameraZoomAble) return;

	SetCameraDistance(TargetCameraDistance + InValue * CameraZoomRate * GetWorld()->GetDeltaSeconds(), false);
}

void AWHPlayerController::AddCameraMovementInput(FVector InDirection, float InValue)
{
	if(!bCameraMoveAble) return;

	SetCameraLocation(TargetCameraLocation + InDirection * CameraMoveRate * InValue * GetWorld()->GetDeltaSeconds(), false);
}

void AWHPlayerController::SetCameraStates(bool bInCameraControlAble, bool bInCameraMoveAble, bool bInCameraRotateAble, bool bInCameraZoomAble)
{
	bCameraControlAble = bInCameraControlAble;
	bCameraMoveAble = bInCameraMoveAble;
	bCameraRotateAble = bInCameraRotateAble;
	bCameraZoomAble = bInCameraZoomAble;
}

float AWHPlayerController::GetCameraDistance(bool bReally)
{
	if(GetCameraBoom())
	{
		return bReally ? GetCameraBoom()->TargetArmLength : TargetCameraDistance;
	}
	return 0.f;
}

USpringArmComponent* AWHPlayerController::GetCameraBoom()
{
	if(!CameraBoom && GetPawn())
	{
		CameraBoom = Cast<USpringArmComponent>(GetPawn()->GetComponentByClass(USpringArmComponent::StaticClass()));
	}
	return CameraBoom;
}

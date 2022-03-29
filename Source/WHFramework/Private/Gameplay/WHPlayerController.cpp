// Fill out your copyright notice in the Description page of Project Settings.

#include "Gameplay/WHPlayerController.h"

#include "Net/UnrealNetwork.h"
#include "Audio/AudioModuleNetworkComponent.h"
#include "Camera/CameraModuleNetworkComponent.h"
#include "Camera/Base/CameraPawnBase.h"
#include "Character/CharacterModuleNetworkComponent.h"
#include "Components/WidgetInteractionComponent.h"
#include "Event/EventModuleNetworkComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Gameplay/WHPlayerInterface.h"
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

	bUseNormalizedZoom = false;

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

	bUse2DInputMode = false;
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

	InputComponent->BindAxis(FName("MoveForward"), this, &AWHPlayerController::MoveForward);
	InputComponent->BindAxis(FName("MoveRight"), this, &AWHPlayerController::MoveRight);
	InputComponent->BindAxis(FName("MoveUp"), this, &AWHPlayerController::MoveUp);

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

	if(GetPawn() && GetPawn()->IsA(ACameraPawnBase::StaticClass()) && !GetPawn()->GetActorLocation().Equals(TargetCameraLocation))
	{
		GetPawn()->SetActorLocation(CameraMoveSmooth == 0 ? TargetCameraLocation : FMath::VInterpTo(GetPawn()->GetActorLocation(), TargetCameraLocation, DeltaTime, CameraMoveSmooth));
	}

	if(!GetControlRotation().Equals(TargetCameraRotation))
	{
		SetControlRotation(CameraRotateSmooth == 0 ? TargetCameraRotation : FMath::RInterpTo(GetControlRotation(), TargetCameraRotation, DeltaTime, CameraRotateSmooth));
	}

	if(GetCameraBoom() && GetCameraBoom()->TargetArmLength != TargetCameraDistance)
	{
		GetCameraBoom()->TargetArmLength = CameraZoomSmooth == 0 ? TargetCameraDistance : FMath::FInterpTo(GetCameraBoom()->TargetArmLength, TargetCameraDistance, DeltaTime, bUseNormalizedZoom ? UKismetMathLibrary::NormalizeToRange(GetCameraBoom()->TargetArmLength, MinCameraDistance, MaxCameraDistance) * CameraZoomSmooth : CameraZoomSmooth);
	}
}
void AWHPlayerController::Turn(float InRate)
{
	if(InRate == 0.f) return;

	if(IsControllingRotate())
	{
		AddCameraRotationInput(InRate, 0.f);
	}
}

void AWHPlayerController::LookUp(float InRate)
{
	if(InRate == 0.f) return;

	if(IsControllingRotate())
	{
		AddCameraRotationInput(0.f, -InRate);
	}
}

void AWHPlayerController::PanH(float InRate)
{
	if(InRate == 0.f) return;

	if(IsControllingMove())
	{
		const FRotator Rotation = GetControlRotation();
		//const FRotator YawRotation(0, Rotation.Yaw, 0);
		const FVector Direction = FRotationMatrix(Rotation).GetUnitAxis(EAxis::Y) * -1.f;
		AddCameraMovementInput(Direction, InRate);
	}
}

void AWHPlayerController::PanV(float InRate)
{
	if(InRate == 0.f) return;

	if(IsControllingMove())
	{
		const FRotator Rotation = GetControlRotation();
		const FVector Direction = FRotationMatrix(Rotation).GetUnitAxis(EAxis::Z) * -1.f;;
		AddCameraMovementInput(Direction, InRate);
	}
}

void AWHPlayerController::Pinch(float InRate)
{
	if(InRate == 0.f) return;
	
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
	if(InRate == 0.f) return;

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
	else if(TouchPressedCount >= 2)
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
			if(!TrackAllowControl || !IsControllingMove())
			{
				SetCameraLocation(TrackTargetActor->GetActorLocation() + TrackTargetActor->GetActorRotation().RotateVector(TrackLocationOffset), bInstant);
			}
			break;
		}
		case ETrackTargetMode::LocationAndRotation:
		{
			if(!TrackAllowControl || !IsControllingMove())
			{
				SetCameraLocation(TrackTargetActor->GetActorLocation() + TrackTargetActor->GetActorRotation().RotateVector(TrackLocationOffset), bInstant);
			}
			if(!TrackAllowControl || !IsControllingRotate())
			{
				SetCameraRotation(TrackTargetActor->GetActorRotation().Yaw + TrackYawOffset, TrackTargetActor->GetActorRotation().Pitch + TrackPitchOffset, bInstant);
			}
			break;
		}
		case ETrackTargetMode::LocationAndRotationAndDistance:
		{
			if(!TrackAllowControl || !IsControllingMove())
			{
				SetCameraLocation(TrackTargetActor->GetActorLocation() + TrackTargetActor->GetActorRotation().RotateVector(TrackLocationOffset), bInstant);
			}
			if(!TrackAllowControl || !IsControllingRotate())
			{
				SetCameraRotation(TrackTargetActor->GetActorRotation().Yaw + TrackYawOffset, TrackTargetActor->GetActorRotation().Pitch + TrackPitchOffset, bInstant);
			}
			if(!TrackAllowControl || !IsControllingZoom())
			{
				SetCameraDistance(TrackDistance, bInstant);
			}
			break;
		}
		case ETrackTargetMode::LocationAndRotationAndDistanceOnce:
		{
			if(!TrackAllowControl || !IsControllingMove())
			{
				SetCameraLocation(TrackTargetActor->GetActorLocation() + TrackTargetActor->GetActorRotation().RotateVector(TrackLocationOffset), bInstant);
			}
			if(!TrackAllowControl || !IsControllingRotate())
			{
				SetCameraRotation(TrackTargetActor->GetActorRotation().Yaw + TrackYawOffset, TrackTargetActor->GetActorRotation().Pitch + TrackPitchOffset, bInstant);
			}
			if(!TrackAllowControl || !IsControllingZoom())
			{
				SetCameraDistance(TrackDistance, bInstant);
			}
			TrackTargetMode = ETrackTargetMode::LocationAndRotation;
			break;
		}
	}
}

void AWHPlayerController::StartTrackTarget(AActor* InTargetActor, ETrackTargetMode InTrackTargetMode, FVector InLocationOffset, float InYawOffset, float InPitchOffset, float InDistance, bool bAllowControl, bool bInstant)
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
		TrackAllowControl = bAllowControl;
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
	if(bInstant && GetPawn() && GetPawn()->IsA(ACameraPawnBase::StaticClass()))
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

bool AWHPlayerController::IsControllingMove() const
{
	return IsInputKeyDown(FKey(TEXT("MiddleMouseButton"))) || GetInputAxisValue(FName("MoveForward")) != 0.f || GetInputAxisValue(FName("MoveRight")) != 0.f || GetInputAxisValue(FName("MoveUp")) != 0.f || TouchPressedCount == 2;
}

bool AWHPlayerController::IsControllingRotate() const
{
	return IsInputKeyDown(FKey(TEXT("RightMouseButton"))) || TouchPressedCount == 1;
}

bool AWHPlayerController::IsControllingZoom() const
{
	return GetInputAxisValue(FName("ZoomCam")) != 0.f || GetInputAxisValue(FName("Pinch")) != 0.f;
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

void AWHPlayerController::SetCameraControlStates(bool bInCameraControlAble, bool bInCameraMoveAble, bool bInCameraRotateAble, bool bInCameraZoomAble)
{
	bCameraControlAble = bInCameraControlAble;
	bCameraMoveAble = bInCameraMoveAble;
	bCameraRotateAble = bInCameraRotateAble;
	bCameraZoomAble = bInCameraZoomAble;
}

void AWHPlayerController::MoveForward(float InValue)
{
	if(InValue == 0.f) return;

	if(IWHPlayerInterface* PlayerInterface = GetPawn<IWHPlayerInterface>())
	{
		PlayerInterface->MoveForward(InValue, bUse2DInputMode);
	}
}

void AWHPlayerController::MoveRight(float InValue)
{
	if(InValue == 0.f) return;

	if(IWHPlayerInterface* PlayerInterface = GetPawn<IWHPlayerInterface>())
	{
		PlayerInterface->MoveRight(InValue, bUse2DInputMode);
	}
}

void AWHPlayerController::MoveUp(float InValue)
{
	if(InValue == 0.f) return;

	if(IWHPlayerInterface* PlayerInterface = GetPawn<IWHPlayerInterface>())
	{
		PlayerInterface->MoveUp(InValue, bUse2DInputMode);
	}
}

// Fill out your copyright notice in the Description page of Project Settings.

#include "Gameplay/WHPlayerController.h"

#include "Net/UnrealNetwork.h"
#include "Audio/AudioModuleNetworkComponent.h"
#include "Camera/CameraModuleNetworkComponent.h"
#include "Camera/Base/CameraPawnBase.h"
#include "Character/CharacterModuleNetworkComponent.h"
#include "Components/WidgetInteractionComponent.h"
#include "Debug/DebugModuleTypes.h"
#include "Event/EventModuleNetworkComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Gameplay/WHPlayerInterface.h"
#include "Kismet/KismetMathLibrary.h"
#include "Math/MathBPLibrary.h"
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
	bCameraMoveControlAble = true;
	bReverseCameraMove = false;
	bClampCameraMove = false;
	CameraMoveLimit = FBox(EForceInit::ForceInitToZero);
	CameraPanMoveKey = FKey("MiddleMouseButton");
	CameraMoveRate = 2000.f;
	CameraMoveSmooth = 5.f;

	bCameraRotateAble = true;
	bCameraRotateControlAble = true;
	CameraRotateKey = FKey("RightMouseButton");
	CameraTurnRate = 90.f;
	CameraLookUpRate = 90.f;
	CameraRotateSmooth = 2.f;
	MinCameraPinch = -89.f;
	MaxCameraPinch = 89.f;
	InitCameraPinch = -10.f;

	bCameraZoomAble = true;
	bCameraZoomControlAble = true;
	bUseNormalizedZoom = false;
	CameraZoomKey = FKey();
	CameraZoomRate = 150.f;
	CameraZoomSmooth = 5.f;
	MinCameraDistance = 100.f;
	MaxCameraDistance = 300.f;
	InitCameraDistance = 150.f;

	CameraDoMoveTime = 0.f;
	CameraDoMoveDuration = 0.f;
	CameraDoMoveEaseType = EEaseType::Linear;
	CameraDoMoveLocation = FVector::ZeroVector;
	CameraDoRotateTime = 0.f;
	CameraDoRotateDuration = 0.f;
	CameraDoRotateRotation = FRotator::ZeroRotator;
	CameraDoRotateEaseType = EEaseType::Linear;
	CameraDoZoomTime = 0.f;
	CameraDoZoomDuration = 0.f;
	CameraDoZoomDistance = 0.f;
	CameraDoZoomEaseType = EEaseType::Linear;

	TrackTargetActor = nullptr;
	TrackLocationOffset = FVector::ZeroVector;
	TrackYawOffset = 0.f;
	TrackPitchOffset = 0.f;
	TrackDistance = 0.f;
	TrackTargetMode = ETrackTargetMode::LocationOnly;

	CurrentCameraLocation = FVector::ZeroVector;
	CurrentCameraRotation = FRotator::ZeroRotator;
	CurrentCameraDistance = 0.f;
	
	TargetCameraLocation = FVector::ZeroVector;
	TargetCameraRotation = FRotator::ZeroRotator;
	TargetCameraDistance = 0.f;

	bUse2DInputMode = false;
	TouchInputRate = 1.f;

	TouchPressedCount = 0;
	TouchLocationPrevious = FVector2D(-1.f, -1.f);
	TouchPinchValuePrevious = -1.f;
}

void AWHPlayerController::Initialize_Implementation()
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
	InputComponent->BindAxis(FName("ZoomCam"), this, &AWHPlayerController::ZoomCam);

	InputComponent->BindAxis(FName("MoveForward"), this, &AWHPlayerController::MoveForward);
	InputComponent->BindAxis(FName("MoveRight"), this, &AWHPlayerController::MoveRight);
	InputComponent->BindAxis(FName("MoveUp"), this, &AWHPlayerController::MoveUp);

	InputComponent->BindAction(FName("Interact"), EInputEvent::IE_Pressed, this, &AWHPlayerController::StartInteract);
	InputComponent->BindAction(FName("Interact"), EInputEvent::IE_Released, this, &AWHPlayerController::EndInteract);

	InputComponent->BindTouch(EInputEvent::IE_Pressed, this, &AWHPlayerController::TouchPressed);
	InputComponent->BindTouch(EInputEvent::IE_Released, this, &AWHPlayerController::TouchReleased);
	InputComponent->BindTouch(EInputEvent::IE_Repeat, this, &AWHPlayerController::TouchMoved);
}

void AWHPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
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

	if(bCameraMoveAble)
	{
		if(GetPawn() && GetPawn()->IsA(ACameraPawnBase::StaticClass()))
		{
			CurrentCameraLocation = GetPawn()->GetActorLocation();
			if(!CurrentCameraLocation.Equals(TargetCameraLocation))
			{
				if(CameraDoMoveDuration != 0.f)
				{
					CameraDoMoveTime = FMath::Clamp(CameraDoMoveTime + DeltaTime, 0.f, CameraDoMoveDuration);
					GetPawn()->SetActorLocation(FMath::Lerp(CameraDoMoveLocation, TargetCameraLocation, UMathBPLibrary::EvaluateByEaseType(CameraDoMoveEaseType, CameraDoMoveTime, CameraDoMoveDuration)));
				}
				else
				{
					GetPawn()->SetActorLocation(CameraMoveSmooth == 0 ? TargetCameraLocation : FMath::VInterpTo(GetPawn()->GetActorLocation(), TargetCameraLocation, DeltaTime, CameraMoveSmooth));
				}
			}
			else if(CameraDoMoveDuration != 0.f)
			{
				StopDoCameraLocation();
			}
		}
	}

	if(bCameraRotateAble)
	{
		CurrentCameraRotation = GetControlRotation();
		if(!CurrentCameraRotation.Equals(TargetCameraRotation))
		{
			if(CameraDoRotateDuration != 0.f)
			{
				CameraDoRotateTime = FMath::Clamp(CameraDoRotateTime + DeltaTime, 0.f, CameraDoRotateDuration);
				SetControlRotation(FMath::Lerp(CameraDoRotateRotation, TargetCameraRotation, UMathBPLibrary::EvaluateByEaseType(CameraDoRotateEaseType, CameraDoRotateTime, CameraDoRotateDuration)));
			}
			else
			{
				SetControlRotation(CameraRotateSmooth == 0 ? TargetCameraRotation : FMath::RInterpTo(GetControlRotation(), TargetCameraRotation, DeltaTime, CameraRotateSmooth));
			}
		}
		else if(CameraDoRotateDuration != 0.f)
		{
			StopDoCameraRotation();
		}
	}

	if(bCameraZoomAble)
	{
		if(GetCameraBoom())
		{
			CurrentCameraDistance = GetCameraBoom()->TargetArmLength;
			if(CurrentCameraDistance != TargetCameraDistance)
			{
				if(CameraDoZoomDuration != 0.f)
				{
					CameraDoZoomTime = FMath::Clamp(CameraDoZoomTime + DeltaTime, 0.f, CameraDoZoomDuration);
					GetCameraBoom()->TargetArmLength = FMath::Lerp(CameraDoZoomDistance, TargetCameraDistance, UMathBPLibrary::EvaluateByEaseType(CameraDoZoomEaseType, CameraDoZoomTime, CameraDoZoomDuration));
				}
				else
				{
					GetCameraBoom()->TargetArmLength = CameraZoomSmooth == 0 ? TargetCameraDistance : FMath::FInterpTo(GetCameraBoom()->TargetArmLength, TargetCameraDistance, DeltaTime, bUseNormalizedZoom ? UKismetMathLibrary::NormalizeToRange(GetCameraBoom()->TargetArmLength, MinCameraDistance, MaxCameraDistance) * CameraZoomSmooth : CameraZoomSmooth);
				}
			}
			else if(CameraDoZoomDuration != 0.f)
			{
				StopDoCameraDistance();
			}
		}
	}
}

void AWHPlayerController::Turn(float InRate)
{
	if(InRate == 0.f) return;

	if(!CameraRotateKey.IsValid() || IsInputKeyDown(CameraRotateKey))
	{
		AddCameraRotationInput(InRate, 0.f);
	}
}

void AWHPlayerController::LookUp(float InRate)
{
	if(InRate == 0.f) return;

	if(!CameraRotateKey.IsValid() || IsInputKeyDown(CameraRotateKey))
	{
		AddCameraRotationInput(0.f, -InRate);
	}
}

void AWHPlayerController::PanH(float InRate)
{
	if(InRate == 0.f) return;

	if(!CameraPanMoveKey.IsValid() || IsInputKeyDown(CameraPanMoveKey))
	{
		const FRotator Rotation = GetControlRotation();
		const FVector Direction = FRotationMatrix(Rotation).GetUnitAxis(EAxis::Y) * (bReverseCameraMove ? -1.f : 1.f);
		AddCameraMovementInput(Direction, InRate);
	}
}

void AWHPlayerController::PanV(float InRate)
{
	if(InRate == 0.f) return;

	if(!CameraPanMoveKey.IsValid() || IsInputKeyDown(CameraPanMoveKey))
	{
		const FRotator Rotation = GetControlRotation();
		const FVector Direction = FRotationMatrix(Rotation).GetUnitAxis(EAxis::Z) * (bReverseCameraMove ? -1.f : 1.f);
		AddCameraMovementInput(Direction, InRate);
	}
}

void AWHPlayerController::ZoomCam(float InRate)
{
	if(InRate == 0.f) return;

	if(!CameraZoomKey.IsValid() || IsInputKeyDown(CameraZoomKey))
	{
		AddCameraDistanceInput(InRate);
	}
}

void AWHPlayerController::TouchPressed(ETouchIndex::Type InTouchIndex, FVector InLocation)
{
	switch (InTouchIndex)
	{
		case ETouchIndex::Touch1:
		{
			if(TouchReleaseTimerHandle1.IsValid())
			{
				TouchReleasedImpl(InTouchIndex);
			}
			TouchPressedImpl();
			break;
		}
		case ETouchIndex::Touch2:
		{
			if(TouchReleaseTimerHandle2.IsValid())
			{
				TouchReleasedImpl(InTouchIndex);
			}
			TouchPressedImpl();
			break;
		}
		case ETouchIndex::Touch3:
		{
			if(TouchReleaseTimerHandle3.IsValid())
			{
				TouchReleasedImpl(InTouchIndex);
			}
			TouchPressedImpl();
			break;
		}
		default: break;
	}
}

void AWHPlayerController::TouchPressedImpl()
{
	TouchPressedCount++;

	TouchLocationPrevious = FVector2D(-1.f, -1.f);
	TouchPinchValuePrevious = -1.f;

	// GEngine->AddOnScreenDebugMessage(0, 1.5f, FColor::Cyan, FString::FromInt(TouchPressedCount));
}

void AWHPlayerController::TouchReleased(ETouchIndex::Type InTouchIndex, FVector InLocation)
{
	switch (InTouchIndex)
	{
		case ETouchIndex::Touch1:
		{
			FTimerDelegate TimerDelegate;
			TimerDelegate.BindUObject(this, &AWHPlayerController::TouchReleasedImpl, InTouchIndex);
			GetWorld()->GetTimerManager().SetTimer(TouchReleaseTimerHandle1, TimerDelegate, 0.15f, false);
			break;
		}
		case ETouchIndex::Touch2:
		{
			FTimerDelegate TimerDelegate;
			TimerDelegate.BindUObject(this, &AWHPlayerController::TouchReleasedImpl, InTouchIndex);
			GetWorld()->GetTimerManager().SetTimer(TouchReleaseTimerHandle2, TimerDelegate, 0.15f, false);
			break;
		}
		case ETouchIndex::Touch3:
		{
			FTimerDelegate TimerDelegate;
			TimerDelegate.BindUObject(this, &AWHPlayerController::TouchReleasedImpl, InTouchIndex);
			GetWorld()->GetTimerManager().SetTimer(TouchReleaseTimerHandle3, TimerDelegate, 0.15f, false);
			break;
		}
		default: break;
	}
}

void AWHPlayerController::TouchReleasedImpl(ETouchIndex::Type InTouchIndex)
{
	TouchPressedCount--;
	if(TouchPressedCount < 0)
	{
		TouchPressedCount = 0;
	}
	
	TouchLocationPrevious = FVector2D(-1.f, -1.f);
	TouchPinchValuePrevious = -1.f;

	switch (InTouchIndex)
	{
		case ETouchIndex::Touch1:
		{
			GetWorld()->GetTimerManager().ClearTimer(TouchReleaseTimerHandle1);
			break;
		}
		case ETouchIndex::Touch2:
		{
			GetWorld()->GetTimerManager().ClearTimer(TouchReleaseTimerHandle2);
			break;
		}
		case ETouchIndex::Touch3:
		{
			GetWorld()->GetTimerManager().ClearTimer(TouchReleaseTimerHandle3);
			break;
		}
		default: break;
	}

	// GEngine->AddOnScreenDebugMessage(0, 1.5f, FColor::Cyan, FString::FromInt(TouchPressedCount));
}

void AWHPlayerController::TouchMoved(ETouchIndex::Type InTouchIndex, FVector InLocation)
{
	if(TouchPressedCount <= 0) return;
	
	if(TouchPressedCount == 1)
	{
		float TouchLocationX = 0.f;
		float TouchLocationY = 0.f;
		bool bIsCurrentPressed = false;
		GetInputTouchState(InTouchIndex, TouchLocationX, TouchLocationY, bIsCurrentPressed);
		
		if(TouchLocationPrevious != FVector2D(-1.f, -1.f))
		{
			AddCameraRotationInput((TouchLocationX - TouchLocationPrevious.X) * TouchInputRate, -(TouchLocationY - TouchLocationPrevious.Y) * TouchInputRate);
		}
		TouchLocationPrevious = FVector2D(TouchLocationX, TouchLocationY);
	}
	else if(TouchPressedCount == 2)
	{
		float TouchLocationX1 = 0.f;
		float TouchLocationY1 = 0.f;
		bool bIsCurrentPressed1 = false;
		GetInputTouchState(ETouchIndex::Touch1, TouchLocationX1, TouchLocationY1, bIsCurrentPressed1);
		
		float TouchLocationX2;
		float TouchLocationY2;
		bool bIsCurrentPressed2;
		GetInputTouchState(ETouchIndex::Touch2, TouchLocationX2, TouchLocationY2, bIsCurrentPressed2);
		
		const float TouchCurrentPinchValue = FVector2D::Distance(FVector2D(TouchLocationX1, TouchLocationY1), FVector2D(TouchLocationX2, TouchLocationY2));
		if(TouchPinchValuePrevious != -1.f)
		{
			AddCameraDistanceInput(-(TouchCurrentPinchValue - TouchPinchValuePrevious) * TouchInputRate);
		}
		TouchPinchValuePrevious = TouchCurrentPinchValue;
	}
	else if(TouchPressedCount == 3)
	{
		float TouchLocationX = 0.f;
		float TouchLocationY = 0.f;
		bool bIsCurrentPressed = false;
		GetInputTouchState(ETouchIndex::Touch1, TouchLocationX, TouchLocationY, bIsCurrentPressed);
		
		if(TouchLocationPrevious != FVector2D(-1.f, -1.f))
		{
			const FRotator Rotation = GetControlRotation();
			const FVector DirectionH = FRotationMatrix(Rotation).GetUnitAxis(EAxis::Y) * (TouchLocationX - TouchLocationPrevious.X);
			const FVector DirectionV = FRotationMatrix(Rotation).GetUnitAxis(EAxis::Z) * -(TouchLocationY - TouchLocationPrevious.Y);
			AddCameraMovementInput(DirectionH + DirectionV, TouchInputRate * (bReverseCameraMove ? -1.f : 1.f));
		}
		TouchLocationPrevious = FVector2D(TouchLocationX, TouchLocationY);
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

void AWHPlayerController::StartTrackTarget(AActor* InTargetActor, ETrackTargetMode InTrackTargetMode, ETrackTargetSpace InTrackTargetSpace, FVector InLocationOffset, float InYawOffset, float InPitchOffset, float InDistance, bool bAllowControl, bool bInstant)
{
	if(!InTargetActor) return;

	if(TrackTargetActor != InTargetActor)
	{
		TrackTargetActor = InTargetActor;
		TrackTargetMode = InTrackTargetMode;
		TrackLocationOffset = InLocationOffset == FVector(-1.f) ? GetTargetCameraLocation() - InTargetActor->GetActorLocation() : InTrackTargetSpace == ETrackTargetSpace::Local ? InLocationOffset : InLocationOffset - InTargetActor->GetActorLocation();
		TrackYawOffset = InYawOffset == -1.f ? GetTargetCameraRotation().Yaw - InTargetActor->GetActorRotation().Yaw : InTrackTargetSpace == ETrackTargetSpace::Local ? InYawOffset : InYawOffset - InTargetActor->GetActorRotation().Yaw;
		TrackPitchOffset = InPitchOffset == -1.f ? GetTargetCameraRotation().Pitch - InTargetActor->GetActorRotation().Pitch : InTrackTargetSpace == ETrackTargetSpace::Local ? InPitchOffset : InPitchOffset - InTargetActor->GetActorRotation().Pitch;
		TrackDistance = InDistance == -1.f ? GetTargetCameraDistance() : InDistance;
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
	TargetCameraLocation = bClampCameraMove ? ClampVector(InLocation, CameraMoveLimit.Min, CameraMoveLimit.Max) : InLocation;
	if(bInstant && GetPawn() && GetPawn()->IsA(ACameraPawnBase::StaticClass()))
	{
		CurrentCameraLocation = TargetCameraLocation;
		GetPawn()->SetActorLocation(TargetCameraLocation);
	}
	StopDoCameraLocation();
}

void AWHPlayerController::DoCameraLocation(FVector InLocation, float InDuration, EEaseType InEaseType)
{
	TargetCameraLocation = bClampCameraMove ? ClampVector(InLocation, CameraMoveLimit.Min, CameraMoveLimit.Max) : InLocation;
	if(InDuration > 0.f)
	{
		CameraDoMoveTime = 0.f;
		CameraDoMoveDuration = InDuration;
		CameraDoMoveLocation = CurrentCameraLocation;
		CameraDoMoveEaseType = InEaseType;
	}
	else if(GetPawn() && GetPawn()->IsA(ACameraPawnBase::StaticClass()))
	{
		CurrentCameraLocation = TargetCameraLocation;
		GetPawn()->SetActorLocation(TargetCameraLocation);
	}
}

void AWHPlayerController::StopDoCameraLocation()
{
	CameraDoMoveTime = 0.f;
	CameraDoMoveDuration = 0.f;
	CameraDoMoveLocation = FVector::ZeroVector;
}

void AWHPlayerController::SetCameraRotation(float InYaw, float InPitch, bool bInstant)
{
	const FRotator CurrentRot = GetControlRotation();
	TargetCameraRotation = FRotator(FMath::Clamp(InPitch == -1.f ? InitCameraPinch : InPitch, MinCameraPinch, MaxCameraPinch), InYaw == -1.f ? CurrentRot.Yaw : InYaw, CurrentRot.Roll);
	if(bInstant)
	{
		CurrentCameraRotation = TargetCameraRotation;
		SetControlRotation(TargetCameraRotation);
	}
	StopDoCameraRotation();
}

void AWHPlayerController::DoCameraRotation(float InYaw, float InPitch, float InDuration, EEaseType InEaseType)
{
	const FRotator CurrentRot = GetControlRotation();
	TargetCameraRotation = FRotator(FMath::Clamp(InPitch == -1.f ? InitCameraPinch : InPitch, MinCameraPinch, MaxCameraPinch), InYaw == -1.f ? CurrentRot.Yaw : InYaw, CurrentRot.Roll);
	if(InDuration > 0.f)
	{
		CameraDoRotateTime = 0.f;
		CameraDoRotateDuration = InDuration;
		CameraDoRotateRotation = CurrentCameraRotation;
		CameraDoRotateEaseType = InEaseType;
	}
	else
	{
		CurrentCameraRotation = TargetCameraRotation;
		SetControlRotation(TargetCameraRotation);
	}
}

void AWHPlayerController::StopDoCameraRotation()
{
	CameraDoRotateTime = 0.f;
	CameraDoRotateDuration = 0.f;
	CameraDoRotateRotation = FRotator::ZeroRotator;
}

void AWHPlayerController::SetCameraDistance(float InDistance, bool bInstant)
{
	TargetCameraDistance = InDistance != -1.f ? FMath::Clamp(InDistance, MinCameraDistance, MaxCameraDistance) : InitCameraDistance;
	if(bInstant && GetCameraBoom())
	{
		CurrentCameraDistance = TargetCameraDistance;
		GetCameraBoom()->TargetArmLength = TargetCameraDistance;
	}
	StopDoCameraDistance();
}

void AWHPlayerController::DoCameraDistance(float InDistance, float InDuration, EEaseType InEaseType)
{
	TargetCameraDistance = InDistance != -1.f ? FMath::Clamp(InDistance, MinCameraDistance, MaxCameraDistance) : InitCameraDistance;
	if(InDuration > 0.f)
	{
		CameraDoZoomTime = 0.f;
		CameraDoZoomDuration = InDuration;
		CameraDoZoomDistance = CurrentCameraDistance;
		CameraDoZoomEaseType = InEaseType;
	}
	else if(GetCameraBoom())
	{
		CurrentCameraDistance = TargetCameraDistance;
		GetCameraBoom()->TargetArmLength = TargetCameraDistance;
	}
}

void AWHPlayerController::StopDoCameraDistance()
{
	CameraDoZoomTime = 0.f;
	CameraDoZoomDuration = 0.f;
	CameraDoZoomDistance = 0.f;
}

void AWHPlayerController::SetCameraRotationAndDistance(float InYaw, float InPitch, float InDistance, bool bInstant)
{
	SetCameraRotation(InYaw, InPitch, bInstant);
	SetCameraDistance(InDistance, bInstant);
}

void AWHPlayerController::DoCameraRotationAndDistance(float InYaw, float InPitch, float InDistance, float InDuration)
{
	DoCameraRotation(InYaw, InPitch, InDuration);
	DoCameraDistance(InDistance, InDuration);
}

void AWHPlayerController::SetCameraTransform(FVector InLocation, float InYaw, float InPitch, float InDistance, bool bInstant)
{
	SetCameraLocation(InLocation, bInstant);
	SetCameraRotation(InYaw, InPitch, bInstant);
	SetCameraDistance(InDistance, bInstant);
}

void AWHPlayerController::DoCameraTransform(FVector InLocation, float InYaw, float InPitch, float InDistance, float InDuration, EEaseType InEaseType)
{
	DoCameraLocation(InLocation, InDuration, InEaseType);
	DoCameraRotation(InYaw, InPitch, InDuration, InEaseType);
	DoCameraDistance(InDistance, InDuration, InEaseType);
}

void AWHPlayerController::StopDoCameraTransform()
{
	StopDoCameraLocation();
	StopDoCameraRotation();
	StopDoCameraDistance();
}

void AWHPlayerController::AddCameraRotationInput(float InYaw, float InPitch)
{
	if(!bCameraRotateAble || !bCameraRotateControlAble) return;

	SetCameraRotation(TargetCameraRotation.Yaw + InYaw * CameraTurnRate * GetWorld()->GetDeltaSeconds(), TargetCameraRotation.Pitch + InPitch * CameraLookUpRate * GetWorld()->GetDeltaSeconds(), false);
}

void AWHPlayerController::AddCameraDistanceInput(float InValue)
{
	if(!bCameraZoomAble || !bCameraZoomControlAble) return;

	SetCameraDistance(TargetCameraDistance + InValue * CameraZoomRate * GetWorld()->GetDeltaSeconds(), false);
}

void AWHPlayerController::AddCameraMovementInput(FVector InDirection, float InValue)
{
	if(!bCameraMoveAble || !bCameraMoveControlAble) return;

	SetCameraLocation(TargetCameraLocation + InDirection * CameraMoveRate * InValue * GetWorld()->GetDeltaSeconds(), false);
}

bool AWHPlayerController::IsControllingMove() const
{
	return !CameraPanMoveKey.IsValid() || IsInputKeyDown(CameraPanMoveKey) || GetInputAxisValue(FName("MoveForward")) != 0.f || GetInputAxisValue(FName("MoveRight")) != 0.f || GetInputAxisValue(FName("MoveUp")) != 0.f || TouchPressedCount == 2;
}

bool AWHPlayerController::IsControllingRotate() const
{
	return !CameraRotateKey.IsValid() || IsInputKeyDown(CameraRotateKey) || TouchPressedCount == 1;
}

bool AWHPlayerController::IsControllingZoom() const
{
	return GetInputAxisValue(FName("ZoomCam")) != 0.f || GetInputAxisValue(FName("PinchGesture")) != 0.f;
}

USpringArmComponent* AWHPlayerController::GetCameraBoom()
{
	if(!CameraBoom && GetPawn())
	{
		CameraBoom = Cast<USpringArmComponent>(GetPawn()->GetComponentByClass(USpringArmComponent::StaticClass()));
	}
	return CameraBoom;
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

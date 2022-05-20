// Fill out your copyright notice in the Description page of Project Settings.


#include "Camera/CameraModule.h"

#include "Camera/Roam/RoamCameraPawn.h"
#include "GameFramework/SpringArmComponent.h"
#include "Gameplay/WHGameMode.h"
#include "Gameplay/WHPlayerController.h"
#include "Global/GlobalBPLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "Main/MainModule.h"
#include "Math/MathBPLibrary.h"
#include "Net/UnrealNetwork.h"

// Sets default values
ACameraModule::ACameraModule()
{
	ModuleName = FName("CameraModule");

	DefaultCameraClass = nullptr;
	DefaultInstantSwitch = false;
	CameraClasses = TArray<TSubclassOf<ACameraPawnBase>>();
	CameraClasses.Add(ARoamCameraPawn::StaticClass());
	Cameras = TArray<ACameraPawnBase*>();
	CameraMap = TMap<FName, ACameraPawnBase*>();
	CurrentCamera = nullptr;
		
	bCameraControlAble = false;

	bCameraMoveAble = true;
	bCameraMoveControlAble = true;
	bReverseCameraMove = false;
	bClampCameraMove = false;
	CameraMoveLimit = FBox(EForceInit::ForceInitToZero);
	CameraPanMoveKey = FKey("MiddleMouseButton");
	CameraMoveRate = 2000.f;
	CameraMoveSpeed = 5.f;

	bCameraRotateAble = true;
	bCameraRotateControlAble = true;
	CameraRotateKey = FKey("RightMouseButton");
	CameraTurnRate = 90.f;
	CameraLookUpRate = 90.f;
	CameraRotateSpeed = 5.f;
	MinCameraPinch = -89.f;
	MaxCameraPinch = 89.f;
	InitCameraPinch = -10.f;

	bCameraZoomAble = true;
	bCameraZoomControlAble = true;
	bUseNormalizedZoom = false;
	CameraZoomKey = FKey();
	CameraZoomRate = 150.f;
	CameraZoomSpeed = 5.f;
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

	PlayerController = nullptr;

	CurrentCameraLocation = FVector::ZeroVector;
	CurrentCameraRotation = FRotator::ZeroRotator;
	CurrentCameraDistance = 0.f;
	
	TargetCameraLocation = FVector::ZeroVector;
	TargetCameraRotation = FRotator::ZeroRotator;
	TargetCameraDistance = 0.f;

	TouchPressedCount = 0;
	TouchLocationPrevious = FVector2D(-1.f, -1.f);
	TouchPinchValuePrevious = -1.f;
}

#if WITH_EDITOR
void ACameraModule::OnGenerate_Implementation()
{
	Super::OnGenerate_Implementation();

	for(auto Class : CameraClasses)
	{
		if(!Class) continue;

		bool bNeedSpawn = true;
		for(const auto Camera : Cameras)
		{
			if(Camera && Camera->IsA(Class))
			{
				Camera->AttachToActor(this, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
				bNeedSpawn = false;
				break;
			}
		}
		if(bNeedSpawn)
		{
			FActorSpawnParameters ActorSpawnParameters;
			ActorSpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if(ACameraPawnBase* CameraPawn = GetWorld()->SpawnActor<ACameraPawnBase>(Class, ActorSpawnParameters))
			{
				CameraPawn->AttachToActor(this, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
				Cameras.Add(CameraPawn);
			}
		}
	}
}

void ACameraModule::OnDestroy_Implementation()
{
	Super::OnDestroy_Implementation();

	for(const auto Camera : Cameras)
	{
		if(Camera)
		{
			Camera->Destroy();
		}
	}
}
#endif

void ACameraModule::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();

	for(auto Iter : Cameras)
	{
		if(Iter && !CameraMap.Contains(Iter->GetCameraName()))
		{
			CameraMap.Add(Iter->GetCameraName(), Iter);
		}
	}
}

void ACameraModule::OnPreparatory_Implementation()
{
	Super::OnPreparatory_Implementation();

	if(DefaultCameraClass)
	{
		SwitchCamera<ACameraPawnBase>(DefaultInstantSwitch, DefaultCameraClass);
	}
}

void ACameraModule::OnRefresh_Implementation(float DeltaSeconds)
{
	Super::OnRefresh_Implementation(DeltaSeconds);

	if(!bCameraControlAble) return;

	DoTrackTarget();

	if(bCameraMoveAble)
	{
		if(CurrentCamera && CurrentCamera->IsA(ACameraPawnBase::StaticClass()))
		{
			CurrentCameraLocation = CurrentCamera->GetActorLocation();
			if(!CurrentCameraLocation.Equals(TargetCameraLocation))
			{
				if(CameraDoMoveDuration != 0.f)
				{
					CameraDoMoveTime = FMath::Clamp(CameraDoMoveTime + DeltaSeconds, 0.f, CameraDoMoveDuration);
					CurrentCamera->SetActorLocation(FMath::Lerp(CameraDoMoveLocation, TargetCameraLocation, UMathBPLibrary::EvaluateByEaseType(CameraDoMoveEaseType, CameraDoMoveTime, CameraDoMoveDuration)));
				}
				else
				{
					CurrentCamera->SetActorLocation(CameraMoveSpeed == 0 ? TargetCameraLocation : FMath::VInterpTo(CurrentCamera->GetActorLocation(), TargetCameraLocation, DeltaSeconds, CameraMoveSpeed));
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
		CurrentCameraRotation = GetPlayerController()->GetControlRotation();
		if(!CurrentCameraRotation.Equals(TargetCameraRotation))
		{
			if(CameraDoRotateDuration != 0.f)
			{
				CameraDoRotateTime = FMath::Clamp(CameraDoRotateTime + DeltaSeconds, 0.f, CameraDoRotateDuration);
				GetPlayerController()->SetControlRotation(FMath::Lerp(CameraDoRotateRotation, TargetCameraRotation, UMathBPLibrary::EvaluateByEaseType(CameraDoRotateEaseType, CameraDoRotateTime, CameraDoRotateDuration)));
			}
			else
			{
				GetPlayerController()->SetControlRotation(CameraRotateSpeed == 0 ? TargetCameraRotation : FMath::RInterpTo(GetPlayerController()->GetControlRotation(), TargetCameraRotation, DeltaSeconds, CameraRotateSpeed));
			}
		}
		else if(CameraDoRotateDuration != 0.f)
		{
			StopDoCameraRotation();
		}
	}

	if(bCameraZoomAble)
	{
		if(GetCurrentCameraBoom())
		{
			CurrentCameraDistance = GetCurrentCameraBoom()->TargetArmLength;
			if(CurrentCameraDistance != TargetCameraDistance)
			{
				if(CameraDoZoomDuration != 0.f)
				{
					CameraDoZoomTime = FMath::Clamp(CameraDoZoomTime + DeltaSeconds, 0.f, CameraDoZoomDuration);
					GetCurrentCameraBoom()->TargetArmLength = FMath::Lerp(CameraDoZoomDistance, TargetCameraDistance, UMathBPLibrary::EvaluateByEaseType(CameraDoZoomEaseType, CameraDoZoomTime, CameraDoZoomDuration));
				}
				else
				{
					GetCurrentCameraBoom()->TargetArmLength = CameraZoomSpeed == 0 ? TargetCameraDistance : FMath::FInterpTo(GetCurrentCameraBoom()->TargetArmLength, TargetCameraDistance, DeltaSeconds, bUseNormalizedZoom ? UKismetMathLibrary::NormalizeToRange(GetCurrentCameraBoom()->TargetArmLength, MinCameraDistance, MaxCameraDistance) * CameraZoomSpeed : CameraZoomSpeed);
				}
			}
			else if(CameraDoZoomDuration != 0.f)
			{
				StopDoCameraDistance();
			}
		}
	}
}

void ACameraModule::OnPause_Implementation()
{
	Super::OnPause_Implementation();
}

void ACameraModule::OnUnPause_Implementation()
{
	Super::OnUnPause_Implementation();
}

ACameraPawnBase* ACameraModule::K2_GetCurrentCamera(TSubclassOf<ACameraPawnBase> InClass)
{
	if(!InClass) return nullptr;
	
	return CurrentCamera;
}

USpringArmComponent* ACameraModule::GetCurrentCameraBoom()
{
	if(CurrentCamera)
	{
		return CurrentCamera->GetCameraBoom();
	}
	return nullptr;
}

ACameraPawnBase* ACameraModule::K2_GetCamera(TSubclassOf<ACameraPawnBase> InClass)
{
	if(!InClass) return nullptr;
	
	const FName CameraName = InClass.GetDefaultObject()->GetCameraName();
	return K2_GetCameraByName(CameraName);
}

ACameraPawnBase* ACameraModule::K2_GetCameraByName(const FName InCameraName) const
{
	if(CameraMap.Contains(InCameraName))
	{
		return CameraMap[InCameraName];
	}
	return nullptr;
}

void ACameraModule::K2_SwitchCamera(TSubclassOf<ACameraPawnBase> InClass, bool bInstant)
{
	if(!InClass) return;
	
	const FName CameraName = InClass.GetDefaultObject()->GetCameraName();
	SwitchCameraByName(CameraName, bInstant);
}

void ACameraModule::SwitchCameraByName(const FName InCameraName, bool bInstant)
{
	if(!CurrentCamera || CurrentCamera->GetCameraName() != InCameraName)
	{
		if(ACameraPawnBase* Camera = GetCameraByName<ACameraPawnBase>(InCameraName))
		{
			PlayerController->Possess(Camera);
			
			SetCameraLocation(Camera->GetActorLocation(), bInstant);
			SetCameraRotation(Camera->GetActorRotation().Yaw, Camera->GetActorRotation().Pitch, bInstant);
			SetCameraDistance(Camera->GetCameraBoom()->TargetArmLength, bInstant);
			CurrentCamera = Camera;
		}
	}
}

void ACameraModule::DoTrackTarget(bool bInstant)
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

void ACameraModule::StartTrackTarget(AActor* InTargetActor, ETrackTargetMode InTrackTargetMode, ETrackTargetSpace InTrackTargetSpace, FVector InLocationOffset, float InYawOffset, float InPitchOffset, float InDistance, bool bAllowControl, bool bInstant)
{
	if(!InTargetActor) return;

	if(TrackTargetActor != InTargetActor)
	{
		TrackTargetActor = InTargetActor;
		TrackTargetMode = InTrackTargetMode;
		TrackLocationOffset = InLocationOffset == FVector(-1.f) ? TargetCameraLocation - InTargetActor->GetActorLocation() : InTrackTargetSpace == ETrackTargetSpace::Local ? InLocationOffset : InLocationOffset - InTargetActor->GetActorLocation();
		TrackYawOffset = InYawOffset == -1.f ? TargetCameraRotation.Yaw - InTargetActor->GetActorRotation().Yaw : InTrackTargetSpace == ETrackTargetSpace::Local ? InYawOffset : InYawOffset - InTargetActor->GetActorRotation().Yaw;
		TrackPitchOffset = InPitchOffset == -1.f ? TargetCameraRotation.Pitch - InTargetActor->GetActorRotation().Pitch : InTrackTargetSpace == ETrackTargetSpace::Local ? InPitchOffset : InPitchOffset - InTargetActor->GetActorRotation().Pitch;
		TrackDistance = InDistance == -1.f ? TargetCameraDistance : InDistance;
		TrackAllowControl = bAllowControl;
		DoTrackTarget(bInstant);
	}
}

void ACameraModule::EndTrackTarget(AActor* InTargetActor)
{
	if(!InTargetActor || InTargetActor == TrackTargetActor)
	{
		TrackTargetActor = nullptr;
	}
}

void ACameraModule::SetCameraLocation(FVector InLocation, bool bInstant)
{
	TargetCameraLocation = bClampCameraMove ? ClampVector(InLocation, CameraMoveLimit.Min, CameraMoveLimit.Max) : InLocation;
	if(bInstant && CurrentCamera && CurrentCamera->IsA(ACameraPawnBase::StaticClass()))
	{
		CurrentCameraLocation = TargetCameraLocation;
		CurrentCamera->SetActorLocation(TargetCameraLocation);
	}
	StopDoCameraLocation();
}

void ACameraModule::DoCameraLocation(FVector InLocation, float InDuration, EEaseType InEaseType)
{
	TargetCameraLocation = bClampCameraMove ? ClampVector(InLocation, CameraMoveLimit.Min, CameraMoveLimit.Max) : InLocation;
	if(InDuration > 0.f)
	{
		CameraDoMoveTime = 0.f;
		CameraDoMoveDuration = InDuration;
		CameraDoMoveLocation = CurrentCameraLocation;
		CameraDoMoveEaseType = InEaseType;
	}
	else if(CurrentCamera && CurrentCamera->IsA(ACameraPawnBase::StaticClass()))
	{
		CurrentCameraLocation = TargetCameraLocation;
		CurrentCamera->SetActorLocation(TargetCameraLocation);
	}
}

void ACameraModule::StopDoCameraLocation()
{
	CameraDoMoveTime = 0.f;
	CameraDoMoveDuration = 0.f;
	CameraDoMoveLocation = FVector::ZeroVector;
}

void ACameraModule::SetCameraRotation(float InYaw, float InPitch, bool bInstant)
{
	const FRotator CurrentRot = GetPlayerController()->GetControlRotation();
	TargetCameraRotation = FRotator(FMath::Clamp(InPitch == -1.f ? InitCameraPinch : InPitch, MinCameraPinch, MaxCameraPinch), InYaw == -1.f ? CurrentRot.Yaw : InYaw, CurrentRot.Roll);
	if(bInstant)
	{
		CurrentCameraRotation = TargetCameraRotation;
		GetPlayerController()->SetControlRotation(TargetCameraRotation);
	}
	StopDoCameraRotation();
}

void ACameraModule::DoCameraRotation(float InYaw, float InPitch, float InDuration, EEaseType InEaseType)
{
	const FRotator CurrentRot = GetPlayerController()->GetControlRotation();
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
		GetPlayerController()->SetControlRotation(TargetCameraRotation);
	}
}

void ACameraModule::StopDoCameraRotation()
{
	CameraDoRotateTime = 0.f;
	CameraDoRotateDuration = 0.f;
	CameraDoRotateRotation = FRotator::ZeroRotator;
}

void ACameraModule::SetCameraDistance(float InDistance, bool bInstant)
{
	TargetCameraDistance = InDistance != -1.f ? FMath::Clamp(InDistance, MinCameraDistance, MaxCameraDistance) : InitCameraDistance;
	if(bInstant && GetCurrentCameraBoom())
	{
		CurrentCameraDistance = TargetCameraDistance;
		GetCurrentCameraBoom()->TargetArmLength = TargetCameraDistance;
	}
	StopDoCameraDistance();
}

void ACameraModule::DoCameraDistance(float InDistance, float InDuration, EEaseType InEaseType)
{
	TargetCameraDistance = InDistance != -1.f ? FMath::Clamp(InDistance, MinCameraDistance, MaxCameraDistance) : InitCameraDistance;
	if(InDuration > 0.f)
	{
		CameraDoZoomTime = 0.f;
		CameraDoZoomDuration = InDuration;
		CameraDoZoomDistance = CurrentCameraDistance;
		CameraDoZoomEaseType = InEaseType;
	}
	else if(GetCurrentCameraBoom())
	{
		CurrentCameraDistance = TargetCameraDistance;
		GetCurrentCameraBoom()->TargetArmLength = TargetCameraDistance;
	}
}

void ACameraModule::StopDoCameraDistance()
{
	CameraDoZoomTime = 0.f;
	CameraDoZoomDuration = 0.f;
	CameraDoZoomDistance = 0.f;
}

void ACameraModule::SetCameraRotationAndDistance(float InYaw, float InPitch, float InDistance, bool bInstant)
{
	SetCameraRotation(InYaw, InPitch, bInstant);
	SetCameraDistance(InDistance, bInstant);
}

void ACameraModule::DoCameraRotationAndDistance(float InYaw, float InPitch, float InDistance, float InDuration, EEaseType InEaseType)
{
	DoCameraRotation(InYaw, InPitch, InDuration, InEaseType);
	DoCameraDistance(InDistance, InDuration, InEaseType);
}

void ACameraModule::SetCameraTransform(FVector InLocation, float InYaw, float InPitch, float InDistance, bool bInstant)
{
	SetCameraLocation(InLocation, bInstant);
	SetCameraRotation(InYaw, InPitch, bInstant);
	SetCameraDistance(InDistance, bInstant);
}

void ACameraModule::DoCameraTransform(FVector InLocation, float InYaw, float InPitch, float InDistance, float InDuration, EEaseType InEaseType)
{
	DoCameraLocation(InLocation, InDuration, InEaseType);
	DoCameraRotation(InYaw, InPitch, InDuration, InEaseType);
	DoCameraDistance(InDistance, InDuration, InEaseType);
}

void ACameraModule::StopDoCameraTransform()
{
	StopDoCameraLocation();
	StopDoCameraRotation();
	StopDoCameraDistance();
}

void ACameraModule::AddCameraRotationInput(float InYaw, float InPitch)
{
	if(!bCameraRotateAble || !bCameraRotateControlAble) return;

	SetCameraRotation(TargetCameraRotation.Yaw + InYaw * CameraTurnRate * GetWorld()->GetDeltaSeconds(), TargetCameraRotation.Pitch + InPitch * CameraLookUpRate * GetWorld()->GetDeltaSeconds(), false);
}

void ACameraModule::AddCameraDistanceInput(float InValue)
{
	if(!bCameraZoomAble || !bCameraZoomControlAble) return;

	SetCameraDistance(TargetCameraDistance + InValue * CameraZoomRate * GetWorld()->GetDeltaSeconds(), false);
}

void ACameraModule::AddCameraMovementInput(FVector InDirection, float InValue)
{
	if(!bCameraMoveAble || !bCameraMoveControlAble) return;

	SetCameraLocation(TargetCameraLocation + InDirection * CameraMoveRate * InValue * GetWorld()->GetDeltaSeconds(), false);
}

bool ACameraModule::IsControllingMove()
{
	return !CameraPanMoveKey.IsValid() || GetPlayerController()->IsInputKeyDown(CameraPanMoveKey) || GetInputAxisValue(FName("MoveForward")) != 0.f || GetInputAxisValue(FName("MoveRight")) != 0.f || GetInputAxisValue(FName("MoveUp")) != 0.f || TouchPressedCount == 2;
}

bool ACameraModule::IsControllingRotate()
{
	return !CameraRotateKey.IsValid() || GetPlayerController()->IsInputKeyDown(CameraRotateKey) || GetPlayerController()->GetTouchPressedCount() == 1;
}

bool ACameraModule::IsControllingZoom()
{
	return GetInputAxisValue(FName("ZoomCam")) != 0.f || GetInputAxisValue(FName("PinchGesture")) != 0.f;
}

FVector ACameraModule::GetRealCameraLocation()
{
	return GetPlayerController()->PlayerCameraManager->GetCameraLocation();
}

FRotator ACameraModule::GetRealCameraRotation()
{
	return GetPlayerController()->PlayerCameraManager->GetCameraRotation();
}

AWHPlayerController* ACameraModule::GetPlayerController()
{
	if(!PlayerController)
	{
		PlayerController = UGlobalBPLibrary::GetPlayerController<AWHPlayerController>(this);
	}
	return PlayerController;
}

void ACameraModule::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

}

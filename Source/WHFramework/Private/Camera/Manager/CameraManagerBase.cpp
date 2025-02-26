// Copyright Epic Games, Inc. All Rights Reserved.

#include "Camera/Manager/CameraManagerBase.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraModule.h"
#include "Camera/Actor/CameraActorBase.h"
#include "Camera/Interface/CameraTrackableInterface.h"
#include "Common/CommonStatics.h"
#include "Event/EventModuleStatics.h"
#include "Event/Handle/Camera/EventHandle_CameraPointChanged.h"
#include "GameFramework/SpringArmComponent.h"
#include "Math/MathHelper.h"
#include "Scene/SceneModuleStatics.h"
#include "Camera/Point/CameraPointBase.h"
#include "Event/Handle/Camera/EventHandle_CameraTraceEnded.h"
#include "Input/InputModuleStatics.h"
#include "Kismet/KismetMathLibrary.h"

ACameraManagerBase::ACameraManagerBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Cameras = TArray<ACameraActorBase*>();
	CameraMap = TMap<FName, ACameraActorBase*>();
	CurrentCamera = nullptr;
	CurrentCameraPoint = nullptr;
	InitCameraOffset = FVector::ZeroVector;
	InitCameraPitch = -1.f;
	InitCameraDistance = -1.f;
	InitCameraFov = 0.f;

	CachedCameraViewData = FCameraViewData();

	CameraDoLocationTime = 0.f;
	CameraDoLocationDuration = 0.f;
	CameraDoLocationEaseType = EEaseType::Linear;
	CameraDoLocationLocation = EMPTY_Vector;
		
	CameraDoOffsetTime = 0.f;
	CameraDoOffsetDuration = 0.f;
	CameraDoOffsetEaseType = EEaseType::Linear;
	CameraDoOffsetOffset = EMPTY_Vector;

	CameraDoRotationTime = 0.f;
	CameraDoRotationDuration = 0.f;
	CameraDoRotationRotation = EMPTY_Rotator;
	CameraDoRotationEaseType = EEaseType::Linear;
	
	CameraDoDistanceTime = 0.f;
	CameraDoDistanceDuration = 0.f;
	CameraDoDistanceDistance = EMPTY_Flt;
	CameraDoDistanceEaseType = EEaseType::Linear;
	
	CameraDoFovTime = 0.f;
	CameraDoFovDuration = 0.f;
	CameraDoFovFov = EMPTY_Flt;
	CameraDoFovEaseType = EEaseType::Linear;

	bTrackAllowControl = false;
	TrackSmoothMode = ECameraSmoothMode::None;
	TrackControlMode = ECameraControlMode::None;
	TrackCameraViewData = FCameraViewData();

	CurrentCameraLocation = FVector::ZeroVector;
	TargetCameraLocation = FVector::ZeroVector;

	CurrentCameraRotation = FRotator::ZeroRotator;
	TargetCameraRotation = FRotator::ZeroRotator;

	CurrentCameraDistance = 0.f;
	TargetCameraDistance = 0.f;

	CurrentCameraOffset = FVector::ZeroVector;
	TargetCameraOffset = FVector::ZeroVector;

	CurrentCameraFov = 0.f;
	TargetCameraFov = 0.f;

	LocalPlayerIndex = 0;
}

void ACameraManagerBase::InitializeFor(APlayerController* PC)
{
	Super::InitializeFor(PC);

	CurrentCameraLocation = GetRealCameraLocation();
	CurrentCameraRotation = GetRealCameraRotation();
	CurrentCameraDistance = GetRealCameraDistance();

	UCameraModule::Get().RegisterCameraManager(this);

	for(auto Iter : Cameras)
	{
		if(InitCameraDistance != -1.f)
		{
			Iter->GetCameraBoom()->TargetArmLength = InitCameraDistance;
		}
		if(InitCameraPitch != -1.f)
		{
			Iter->SetActorRotation(FRotator(InitCameraPitch, Iter->GetActorRotation().Yaw, Iter->GetActorRotation().Roll));
		}
		CameraMap.Emplace(Iter->GetCameraName(), Iter);
	}
}

void ACameraManagerBase::Destroyed()
{
	Super::Destroyed();

	UCameraModule::Get().UnRegisterCameraManager(this);
}

void ACameraManagerBase::SetViewTarget(AActor* NewViewTarget, FViewTargetTransitionParams TransitionParams)
{
	if(CurrentCamera && (!NewViewTarget || !NewViewTarget->IsA<ACameraActor>()))
	{
		NewViewTarget = CurrentCamera;
	}
	Super::SetViewTarget(NewViewTarget, TransitionParams);
}

void ACameraManagerBase::UpdateCamera(float DeltaTime)
{
	Super::UpdateCamera(DeltaTime);

	DeltaTime /= UCommonStatics::GetTimeScale();

	DoTrackTarget();

	if(UCameraModule::Get().IsCameraMoveAble() && CurrentCamera)
	{
		if(CurrentCameraLocation != TargetCameraLocation)
		{
			if(CameraDoLocationDuration != 0.f)
			{
				CameraDoLocationTime = FMath::Clamp(CameraDoLocationTime + DeltaTime, 0.f, CameraDoLocationDuration);
				CurrentCamera->SetCameraLocation(FMath::Lerp(CameraDoLocationLocation, TargetCameraLocation, FMathHelper::EvaluateByEaseType(CameraDoLocationEaseType, CameraDoLocationTime, CameraDoLocationDuration)));
			}
			else
			{
				CurrentCamera->SetCameraLocation(!UCameraModule::Get().IsSmoothCameraMove() ? TargetCameraLocation : FMath::VInterpTo(CurrentCameraLocation, TargetCameraLocation, DeltaTime, UCameraModule::Get().GetCameraMoveSpeed()));
			}
			CurrentCameraLocation = CurrentCamera->GetActorLocation();
		}
		else if(CameraDoLocationDuration != 0.f)
		{
			StopDoCameraLocation();
		}
	}

	if(UCameraModule::Get().IsCameraOffsetAble() && CurrentCamera)
	{
		if(CurrentCameraOffset != TargetCameraOffset)
		{
			if(CameraDoOffsetDuration != 0.f)
			{
				CameraDoOffsetTime = FMath::Clamp(CameraDoOffsetTime + DeltaTime, 0.f, CameraDoOffsetDuration);
				CurrentCamera->GetCameraBoom()->SocketOffset = FMath::Lerp(CameraDoOffsetOffset, TargetCameraOffset, FMathHelper::EvaluateByEaseType(CameraDoOffsetEaseType, CameraDoOffsetTime, CameraDoOffsetDuration));
			}
			else
			{
				CurrentCamera->GetCameraBoom()->SocketOffset = !UCameraModule::Get().IsSmoothCameraOffset() ? TargetCameraOffset : FMath::VInterpTo(CurrentCameraOffset, TargetCameraOffset, DeltaTime, UCameraModule::Get().GetCameraOffsetSpeed());
			}
			CurrentCameraOffset = CurrentCamera->GetCameraBoom()->SocketOffset;
		}
		else if(CameraDoOffsetDuration != 0.f)
		{
			StopDoCameraOffset();
		}
	}

	if(UCameraModule::Get().IsCameraRotateAble() && PCOwner)
	{
		if(!CurrentCameraRotation.Equals(TargetCameraRotation))
		{
			if(CameraDoRotationDuration != 0.f)
			{
				CameraDoRotationTime = FMath::Clamp(CameraDoRotationTime + DeltaTime, 0.f, CameraDoRotationDuration);
				PCOwner->SetControlRotation(FMathHelper::LerpRotator(CameraDoRotationRotation, TargetCameraRotation, FMathHelper::EvaluateByEaseType(CameraDoRotationEaseType, CameraDoRotationTime, CameraDoRotationDuration), !CameraDoRotationRotation.Equals(TargetCameraRotation)));
			}
			else
			{
				PCOwner->SetControlRotation(!UCameraModule::Get().IsSmoothCameraRotate() ? TargetCameraRotation : FMath::RInterpTo(CurrentCameraRotation, TargetCameraRotation, DeltaTime, UCameraModule::Get().GetCameraRotateSpeed()));
			}
			CurrentCameraRotation = PCOwner->GetControlRotation();
		}
		else if(CameraDoRotationDuration != 0.f)
		{
			StopDoCameraRotation();
		}
		if(CurrentCamera)
		{
			CurrentCamera->SetActorRotation(PCOwner->GetControlRotation());
		}
	}

	if(UCameraModule::Get().IsCameraZoomAble() && CurrentCamera)
	{
		float TargetDistance = TargetCameraDistance;

		while(UCameraModule::Get().GetCameraMoveRange().IsValid && !UCameraModule::Get().GetCameraMoveRange().IsInsideOrOn(CurrentCameraLocation - CurrentCameraRotation.Vector() * TargetDistance) && TargetDistance > 0.f)
		{
			TargetDistance = FMath::Max(0.f, TargetDistance - 100.f);
		}
		
		if(CurrentCameraDistance != TargetDistance)
		{
			if(CameraDoDistanceDuration != 0.f)
			{
				CameraDoDistanceTime = FMath::Clamp(CameraDoDistanceTime + DeltaTime, 0.f, CameraDoDistanceDuration);
				CurrentCamera->GetCameraBoom()->TargetArmLength = FMath::Lerp(CameraDoDistanceDistance, TargetDistance, FMathHelper::EvaluateByEaseType(CameraDoDistanceEaseType, CameraDoDistanceTime, CameraDoDistanceDuration));
			}
			else
			{
				CurrentCamera->GetCameraBoom()->TargetArmLength = !UCameraModule::Get().IsSmoothCameraZoom() ? TargetDistance : FMath::FInterpTo(CurrentCameraDistance, TargetDistance, DeltaTime, UCameraModule::Get().IsNormalizeCameraZoom() && UCameraModule::Get().GetMaxCameraDistance() != -1.f ? UKismetMathLibrary::NormalizeToRange(CurrentCamera->GetCameraBoom()->TargetArmLength, UCameraModule::Get().GetMinCameraDistance(), UCameraModule::Get().GetMaxCameraDistance()) * UCameraModule::Get().GetCameraZoomSpeed() : UCameraModule::Get().GetCameraZoomSpeed());
			}
			CurrentCameraDistance = CurrentCamera->GetCameraBoom()->TargetArmLength;
		}
		else if(CameraDoDistanceDuration != 0.f)
		{
			StopDoCameraDistance();
		}
	}

	if(CurrentCamera)
	{
		if(CurrentCameraFov != TargetCameraFov)
		{
			if(CameraDoFovDuration != 0.f)
			{
				CameraDoFovTime = FMath::Clamp(CameraDoFovTime + DeltaTime, 0.f, CameraDoFovDuration);
				CurrentCamera->GetCamera()->SetFieldOfView(FMath::Lerp(CameraDoFovFov, TargetCameraFov, FMathHelper::EvaluateByEaseType(CameraDoFovEaseType, CameraDoFovTime, CameraDoFovDuration)));
			}
			CurrentCameraFov = CurrentCamera->GetCamera()->FieldOfView;
		}
		else if(CameraDoFovDuration != 0.f)
		{
			StopDoCameraFov();
		}
	}

	if(CurrentCamera && CurrentCamera->GetCamera()->ProjectionMode == ECameraProjectionMode::Orthographic)
	{
		CurrentCamera->GetCamera()->SetOrthoWidth(USceneModuleStatics::GetAltitude(false, true) * CurrentCamera->GetCameraOrthoFactor());
	}
}

void ACameraManagerBase::UpdateViewTarget(FTViewTarget& OutVT, float DeltaTime)
{
	Super::UpdateViewTarget(OutVT, DeltaTime);
}

ACameraActorBase* ACameraManagerBase::GetCurrentCamera(TSubclassOf<ACameraActorBase> InClass) const
{
	return GetDeterminesOutputObject(CurrentCamera, InClass);
}

ACameraActorBase* ACameraManagerBase::GetCameraByClass(TSubclassOf<ACameraActorBase> InClass)
{
	if(!InClass) return nullptr;
	
	const FName CameraName = InClass.GetDefaultObject()->GetCameraName();
	return GetCameraByName(CameraName);
}

ACameraActorBase* ACameraManagerBase::GetCameraByName(const FName InName) const
{
	if(CameraMap.Contains(InName))
	{
		return CameraMap[InName];
	}
	return nullptr;
}

void ACameraManagerBase::SwitchCamera(ACameraActorBase* InCamera, bool bReset, bool bInstant)
{
	if(CurrentCamera == InCamera) return;

	if(InCamera)
	{
		if(CurrentCamera)
		{
			CurrentCamera->OnUnSwitch();
		}
		CurrentCamera = InCamera;
		CurrentCamera->OnSwitch();
		if(PCOwner)
		{
			PCOwner->SetViewTarget(InCamera);
		}
		if(bReset)
		{
			SetCameraLocation(InCamera->GetActorLocation(), bInstant);
			SetCameraRotation(InCamera->GetActorRotation().Yaw, InCamera->GetActorRotation().Pitch, bInstant);
			SetCameraDistance(InCamera->GetCameraBoom()->TargetArmLength, bInstant);
			SetCameraOffset(InitCameraOffset = CurrentCamera->GetCameraBoom()->SocketOffset, bInstant);
			SetCameraFov(InitCameraFov = CurrentCamera->GetCamera()->FieldOfView, bInstant);
		}
		else
		{
			SetCameraLocation(CurrentCameraLocation, bInstant);
			SetCameraRotation(CurrentCameraRotation.Yaw, CurrentCameraRotation.Pitch, bInstant);
			SetCameraDistance(CurrentCameraDistance, bInstant);
			SetCameraOffset(CurrentCameraOffset, bInstant);
			SetCameraFov(CurrentCameraFov, bInstant);
		}
	}
	else if(CurrentCamera)
	{
		if(PCOwner)
		{
			PCOwner->SetViewTarget(nullptr);
		}
		CurrentCamera->OnUnSwitch();
		CurrentCamera = nullptr;
	}
}

void ACameraManagerBase::SwitchCameraByClass(TSubclassOf<ACameraActorBase> InClass, bool bReset, bool bInstant)
{
	const FName CameraName = InClass ? InClass.GetDefaultObject()->GetCameraName() : NAME_None;
	SwitchCameraByName(CameraName, bReset, bInstant);
}

void ACameraManagerBase::SwitchCameraByName(const FName InName, bool bReset, bool bInstant)
{
	SwitchCamera(GetCameraByName(InName), bReset, bInstant);
}

void ACameraManagerBase::SwitchCameraPoint(ACameraPointBase* InCameraPoint, bool bSetAsDefault, bool bInstant)
{
	if(CurrentCameraPoint != InCameraPoint)
	{
		CurrentCameraPoint = InCameraPoint;
		UEventModuleStatics::BroadcastEvent<UEventHandle_CameraPointChanged>(this, { CurrentCameraPoint });
	}
	SetCameraView(InCameraPoint->GetCameraViewData(), true, bInstant);
	if(bSetAsDefault)
	{
		SetDefaultCameraPoint(InCameraPoint);
	}
}

void ACameraManagerBase::DoTrackTarget(bool bInstant)
{
	if(!TrackCameraViewData.CameraViewParams.CameraViewTarget) return;
	
	switch(TrackCameraViewData.TrackTargetMode)
	{
		case ECameraTrackMode::LocationOnly:
		{
			DoTrackTargetLocation(bInstant);
			break;
		}
		case ECameraTrackMode::LocationAndRotation:
		{
			DoTrackTargetLocation(bInstant);
			DoTrackTargetRotation(bInstant);
			break;
		}
		case ECameraTrackMode::LocationAndRotationOnce:
		{
			DoTrackTargetLocation(bInstant);
			DoTrackTargetRotation(bInstant);
			TrackCameraViewData.TrackTargetMode = ECameraTrackMode::LocationOnly;
			break;
		}
		case ECameraTrackMode::LocationAndRotationAndDistance:
		{
			DoTrackTargetLocation(bInstant);
			DoTrackTargetRotation(bInstant);
			DoTrackTargetDistance(bInstant);
			break;
		}
		case ECameraTrackMode::LocationAndRotationAndDistanceOnce:
		{
			DoTrackTargetLocation(bInstant);
			DoTrackTargetRotation(bInstant);
			DoTrackTargetDistance(bInstant);
			TrackCameraViewData.TrackTargetMode = ECameraTrackMode::LocationAndRotation;
			break;
		}
		case ECameraTrackMode::LocationAndRotationOnceAndDistanceOnce:
		{
			DoTrackTargetLocation(bInstant);
			DoTrackTargetRotation(bInstant);
			DoTrackTargetDistance(bInstant);
			TrackCameraViewData.TrackTargetMode = ECameraTrackMode::LocationOnly;
			break;
		}
	}
}

void ACameraManagerBase::DoTrackTargetLocation(bool bInstant)
{
	if(!TrackCameraViewData.CameraViewParams.CameraViewTarget) return;

	if(!bTrackAllowControl || !IsControllingMove())
	{
		switch(TrackCameraViewData.CameraViewParams.CameraViewMode)
		{
			case ECameraViewMode::Instant:
			{
				SetCameraLocation(TrackCameraViewData.CameraViewParams.CameraViewTarget->GetActorLocation() + TrackCameraViewData.CameraViewParams.CameraViewTarget->GetActorRotation().RotateVector(TrackCameraViewData.CameraViewParams.CameraViewLocation), true);
				break;
			}
			case ECameraViewMode::Smooth:
			{
				SetCameraLocation(TrackCameraViewData.CameraViewParams.CameraViewTarget->GetActorLocation() + TrackCameraViewData.CameraViewParams.CameraViewTarget->GetActorRotation().RotateVector(TrackCameraViewData.CameraViewParams.CameraViewLocation), !ENUMWITH(TrackSmoothMode, ECameraSmoothMode::LocationOnly) || TrackCameraViewData.CameraViewParams.CameraViewMode == ECameraViewMode::Instant || bInstant);
				break;
			}
			case ECameraViewMode::Duration:
			{
				DoCameraLocation(TrackCameraViewData.CameraViewParams.CameraViewTarget->GetActorLocation() + TrackCameraViewData.CameraViewParams.CameraViewTarget->GetActorRotation().RotateVector(TrackCameraViewData.CameraViewParams.CameraViewLocation), bInstant ? 0.f : TrackCameraViewData.CameraViewParams.CameraViewDuration, TrackCameraViewData.CameraViewParams.CameraViewEaseType, false);
				break;
			}
			default: break;
		}
	}
}

void ACameraManagerBase::DoTrackTargetRotation(bool bInstant)
{
	if(!TrackCameraViewData.CameraViewParams.CameraViewTarget) return;

	if(!bTrackAllowControl || !IsControllingRotate())
	{
		switch(TrackCameraViewData.CameraViewParams.CameraViewMode)
		{
			case ECameraViewMode::Instant:
			{
				SetCameraRotation(TrackCameraViewData.CameraViewParams.CameraViewTarget->GetActorRotation().Yaw + TrackCameraViewData.CameraViewParams.CameraViewYaw, TrackCameraViewData.CameraViewParams.CameraViewTarget->GetActorRotation().Pitch + TrackCameraViewData.CameraViewParams.CameraViewPitch, true);
				break;
			}
			case ECameraViewMode::Smooth:
			{
				SetCameraRotation(TrackCameraViewData.CameraViewParams.CameraViewTarget->GetActorRotation().Yaw + TrackCameraViewData.CameraViewParams.CameraViewYaw, TrackCameraViewData.CameraViewParams.CameraViewTarget->GetActorRotation().Pitch + TrackCameraViewData.CameraViewParams.CameraViewPitch, !ENUMWITH(TrackSmoothMode, ECameraSmoothMode::RotationOnly) || TrackCameraViewData.CameraViewParams.CameraViewMode == ECameraViewMode::Instant || bInstant);
				break;
			}
			case ECameraViewMode::Duration:
			{
				DoCameraRotation(TrackCameraViewData.CameraViewParams.CameraViewTarget->GetActorRotation().Yaw + TrackCameraViewData.CameraViewParams.CameraViewYaw, TrackCameraViewData.CameraViewParams.CameraViewTarget->GetActorRotation().Pitch + TrackCameraViewData.CameraViewParams.CameraViewPitch, bInstant ? 0.f : TrackCameraViewData.CameraViewParams.CameraViewDuration, TrackCameraViewData.CameraViewParams.CameraViewEaseType, false);
				break;
			}
			default: break;
		}
	}
}

void ACameraManagerBase::DoTrackTargetDistance(bool bInstant)
{
	if(!TrackCameraViewData.CameraViewParams.CameraViewTarget) return;

	if(!bTrackAllowControl || !IsControllingZoom())
	{
		switch(TrackCameraViewData.CameraViewParams.CameraViewMode)
		{
			case ECameraViewMode::Instant:
			{
				SetCameraDistance(TrackCameraViewData.CameraViewParams.CameraViewDistance, true);
				break;
			}
			case ECameraViewMode::Smooth:
			{
				SetCameraDistance(TrackCameraViewData.CameraViewParams.CameraViewDistance, !ENUMWITH(TrackSmoothMode, ECameraSmoothMode::DistanceOnly) || TrackCameraViewData.CameraViewParams.CameraViewMode == ECameraViewMode::Instant || bInstant);
				break;
			}
			case ECameraViewMode::Duration:
			{
				DoCameraDistance(TrackCameraViewData.CameraViewParams.CameraViewDistance, bInstant ? 0.f : TrackCameraViewData.CameraViewParams.CameraViewDuration, TrackCameraViewData.CameraViewParams.CameraViewEaseType, false);
				break;
			}
			default: break;
		}
	}
}

void ACameraManagerBase::StartTrackTarget(AActor* InTargetActor, ECameraTrackMode InTrackMode, ECameraViewMode InViewMode, ECameraViewSpace InViewSpace, FVector InLocation, FVector InOffset, float InYaw, float InPitch, float InDistance, bool bAllowControl, EEaseType InViewEaseType, float InViewDuration, bool bInstant)
{
	if(!InTargetActor || !CurrentCamera || InViewMode == ECameraViewMode::None) return;

	if(TrackCameraViewData.CameraViewParams.CameraViewTarget != InTargetActor)
	{
		TrackSmoothMode = InTargetActor->Implements<UCameraTrackableInterface>() ? ICameraTrackableInterface::Execute_GetCameraSmoothMode(InTargetActor) : ECameraSmoothMode::All;
		TrackControlMode = InTargetActor->Implements<UCameraTrackableInterface>() ? ICameraTrackableInterface::Execute_GetCameraControlMode(InTargetActor) : ECameraControlMode::All;
		TrackCameraViewData.CameraViewTarget = InTargetActor;
		TrackCameraViewData.bTrackTarget = true;
		TrackCameraViewData.TrackTargetMode = InTrackMode;
		TrackCameraViewData.CameraViewParams.CameraViewTarget = InTargetActor;
		TrackCameraViewData.CameraViewParams.CameraViewMode = InViewMode;
		TrackCameraViewData.CameraViewParams.CameraViewSpace = InViewSpace;
		TrackCameraViewData.CameraViewParams.CameraViewLocation = InLocation == FVector(-1.f) ? TargetCameraLocation - InTargetActor->GetActorLocation() : InViewSpace == ECameraViewSpace::Local ? InLocation : InLocation - InTargetActor->GetActorLocation();
		TrackCameraViewData.CameraViewParams.CameraViewYaw = InYaw == -1.f ? TargetCameraRotation.Yaw - InTargetActor->GetActorRotation().Yaw : InViewSpace == ECameraViewSpace::Local ? InYaw : InYaw - InTargetActor->GetActorRotation().Yaw;
		TrackCameraViewData.CameraViewParams.CameraViewPitch = InPitch == -1.f ? TargetCameraRotation.Pitch - InTargetActor->GetActorRotation().Pitch : InViewSpace == ECameraViewSpace::Local ? InPitch : InPitch - InTargetActor->GetActorRotation().Pitch;
		TrackCameraViewData.CameraViewParams.CameraViewDistance = InDistance == -1.f ? TargetCameraDistance : InDistance;
		TrackCameraViewData.CameraViewParams.CameraViewEaseType = InViewEaseType;
		TrackCameraViewData.CameraViewParams.CameraViewDuration = InViewDuration;
		TrackCameraViewData.CameraViewParams.CameraViewOffset = TargetCameraOffset = InOffset == FVector(-1.f) ? InitCameraOffset : InOffset;
		bTrackAllowControl = bAllowControl;
		DoTrackTarget(bInstant);
	}
}

void ACameraManagerBase::EndTrackTarget(AActor* InTargetActor)
{
	if(!CurrentCamera) return;
	
	if(!InTargetActor || InTargetActor == TrackCameraViewData.CameraViewParams.CameraViewTarget)
	{
		if(TrackCameraViewData.CameraViewParams.CameraViewTarget)
		{
			UEventModuleStatics::BroadcastEvent<UEventHandle_CameraTraceEnded>(this, { TrackCameraViewData.CameraViewParams.CameraViewTarget });

			TrackCameraViewData.CameraViewParams.CameraViewTarget = nullptr;
			SetCameraOffset(InitCameraOffset);
		}
	}
}

void ACameraManagerBase::SetCameraLocation(FVector InLocation, bool bInstant)
{
	if(!CurrentCamera) return;

	TargetCameraLocation = UCameraModule::Get().GetCameraMoveRange().IsValid && !IsTrackingTarget() ? ClampVector(InLocation, UCameraModule::Get().GetCameraMoveRange().Min, UCameraModule::Get().GetCameraMoveRange().Max) : InLocation;
	if(bInstant)
	{
		CurrentCameraLocation = TargetCameraLocation;
		CurrentCamera->SetCameraLocation(TargetCameraLocation);
	}
	StopDoCameraLocation();
}

void ACameraManagerBase::DoCameraLocation(FVector InLocation, float InDuration, EEaseType InEaseType, bool bForce)
{
	if(!CurrentCamera || ((CameraDoLocationLocation != EMPTY_Vector || CurrentCameraLocation == InLocation) && !bForce)) return;

	TargetCameraLocation = UCameraModule::Get().GetCameraMoveRange().IsValid && !IsTrackingTarget() ? ClampVector(InLocation, UCameraModule::Get().GetCameraMoveRange().Min, UCameraModule::Get().GetCameraMoveRange().Max) : InLocation;
	if(InDuration > 0.f)
	{
		CameraDoLocationTime = 0.f;
		CameraDoLocationDuration = InDuration;
		CameraDoLocationLocation = CurrentCameraLocation;
		CameraDoLocationEaseType = InEaseType;
	}
	else if(CurrentCamera)
	{
		CurrentCameraLocation = TargetCameraLocation;
		CurrentCamera->SetCameraLocation(TargetCameraLocation);
	}
}

void ACameraManagerBase::StopDoCameraLocation()
{
	CameraDoLocationTime = 0.f;
	CameraDoLocationDuration = 0.f;
	CameraDoLocationLocation = EMPTY_Vector;
}

void ACameraManagerBase::SetCameraOffset(FVector InOffset, bool bInstant)
{
	if(!CurrentCamera) return;

	InOffset = InOffset != FVector(-1.f) ? InOffset : (IsTrackingTarget() ? TrackCameraViewData.CameraViewParams.CameraViewOffset : InitCameraOffset);

	TargetCameraOffset = UCameraModule::Get().GetCameraMoveRange().IsValid && !IsTrackingTarget() ? ClampVector(InOffset, UCameraModule::Get().GetCameraMoveRange().Min, UCameraModule::Get().GetCameraMoveRange().Max) : InOffset;
	if(bInstant)
	{
		CurrentCameraOffset = TargetCameraOffset;
		CurrentCamera->GetCameraBoom()->SocketOffset = TargetCameraOffset;
	}
	StopDoCameraOffset();
}

void ACameraManagerBase::DoCameraOffset(FVector InOffset, float InDuration, EEaseType InEaseType, bool bForce)
{
	if(!CurrentCamera) return;

	InOffset = InOffset != FVector(-1.f) ? InOffset : (IsTrackingTarget() ? TrackCameraViewData.CameraViewParams.CameraViewOffset : InitCameraOffset);

	if((CameraDoOffsetOffset != EMPTY_Vector || CurrentCameraOffset == InOffset) && !bForce) return;

	TargetCameraOffset = InOffset;
	if(InDuration > 0.f)
	{
		CameraDoOffsetTime = 0.f;
		CameraDoOffsetDuration = InDuration;
		CameraDoOffsetOffset = CurrentCameraOffset;
		CameraDoOffsetEaseType = InEaseType;
	}
	else if(CurrentCamera)
	{
		CurrentCameraOffset = TargetCameraOffset;
		CurrentCamera->GetCameraBoom()->SocketOffset = TargetCameraOffset;
	}
}

void ACameraManagerBase::StopDoCameraOffset()
{
	CameraDoOffsetTime = 0.f;
	CameraDoOffsetDuration = 0.f;
	CameraDoOffsetOffset = EMPTY_Vector;
}

void ACameraManagerBase::SetCameraRotation(float InYaw, float InPitch, bool bInstant)
{
	if(!PCOwner) return;
	
	const FRotator TargetRotator = FRotator(FMath::Clamp(InPitch == -1.f ? (InitCameraPitch == -1.f ? CurrentCameraRotation.Pitch : InitCameraPitch) : InPitch, GetMinCameraPitch(), GetMaxCameraPitch()), InYaw == -1.f ? CurrentCameraRotation.Yaw : InYaw, CurrentCameraRotation.Roll);

	if(TargetRotator.Equals(TargetCameraRotation)) return;

	TargetCameraRotation = TargetRotator;

	if(bInstant)
	{
		CurrentCameraRotation = TargetCameraRotation;
		PCOwner->SetControlRotation(TargetCameraRotation);
		if(CurrentCamera)
		{
			CurrentCamera->SetActorRotation(TargetCameraRotation);
		}
	}
	StopDoCameraRotation();
}

void ACameraManagerBase::DoCameraRotation(float InYaw, float InPitch, float InDuration, EEaseType InEaseType, bool bForce)
{
	if(!PCOwner || (CameraDoRotationRotation != EMPTY_Rotator || (CurrentCameraRotation.Yaw == InYaw && CurrentCameraRotation.Pitch == InPitch))  && !bForce) return;

	const FRotator TargetRotator = FRotator(FMath::Clamp(InPitch == -1.f ? (InitCameraPitch == -1.f ? CurrentCameraRotation.Pitch : InitCameraPitch) : InPitch, GetMinCameraPitch(), GetMaxCameraPitch()), InYaw == -1.f ? CurrentCameraRotation.Yaw : InYaw, CurrentCameraRotation.Roll);

	if(TargetRotator.Equals(TargetCameraRotation)) return;

	TargetCameraRotation = TargetRotator;
	
	if(InDuration > 0.f)
	{
		CameraDoRotationTime = 0.f;
		CameraDoRotationDuration = InDuration;
		CameraDoRotationRotation = CurrentCameraRotation;
		CameraDoRotationEaseType = InEaseType;
	}
	else
	{
		CurrentCameraRotation = TargetCameraRotation;
		PCOwner->SetControlRotation(TargetCameraRotation);
		if(CurrentCamera)
		{
			CurrentCamera->SetActorRotation(TargetCameraRotation);
		}
	}
}

void ACameraManagerBase::StopDoCameraRotation()
{
	CameraDoRotationTime = 0.f;
	CameraDoRotationDuration = 0.f;
	CameraDoRotationRotation = EMPTY_Rotator;
}

void ACameraManagerBase::SetCameraDistance(float InDistance, bool bInstant)
{
	if(!CurrentCamera) return;

	TargetCameraDistance = InDistance != -1.f ? FMath::Clamp(InDistance, UCameraModule::Get().GetMinCameraDistance(), UCameraModule::Get().GetMaxCameraDistance() == -1.f ? FLT_MAX : UCameraModule::Get().GetMaxCameraDistance()) : InitCameraDistance;
	while(UCameraModule::Get().GetCameraMoveRange().IsValid && !UCameraModule::Get().GetCameraMoveRange().IsInsideOrOn(CurrentCameraLocation - CurrentCameraRotation.Vector() * TargetCameraDistance) && TargetCameraDistance > 0.f)
	{
		TargetCameraDistance = FMath::Max(0.f, TargetCameraDistance - 100.f);
	}
	if(bInstant)
	{
		CurrentCameraDistance = TargetCameraDistance;
		CurrentCamera->GetCameraBoom()->TargetArmLength = TargetCameraDistance;
	}
	StopDoCameraDistance();
}

void ACameraManagerBase::DoCameraDistance(float InDistance, float InDuration, EEaseType InEaseType, bool bForce)
{
	if(!CurrentCamera || (CameraDoDistanceDistance != EMPTY_Flt || CurrentCameraDistance == InDistance)  && !bForce) return;

	TargetCameraDistance = InDistance != -1.f ? FMath::Clamp(InDistance, UCameraModule::Get().GetMinCameraDistance(), UCameraModule::Get().GetMaxCameraDistance() == -1.f ? FLT_MAX : UCameraModule::Get().GetMaxCameraDistance()) : (IsTrackingTarget() ? TrackCameraViewData.CameraViewParams.CameraViewDistance : InitCameraDistance);
	if(InDuration > 0.f)
	{
		CameraDoDistanceTime = 0.f;
		CameraDoDistanceDuration = InDuration;
		CameraDoDistanceDistance = CurrentCameraDistance;
		CameraDoDistanceEaseType = InEaseType;
	}
	else
	{
		CurrentCameraDistance = TargetCameraDistance;
		CurrentCamera->GetCameraBoom()->TargetArmLength = TargetCameraDistance;
	}
}

void ACameraManagerBase::StopDoCameraDistance()
{
	CameraDoDistanceTime = 0.f;
	CameraDoDistanceDuration = 0.f;
	CameraDoDistanceDistance = EMPTY_Flt;
}

void ACameraManagerBase::SetCameraRotationAndDistance(float InYaw, float InPitch, float InDistance, bool bInstant)
{
	SetCameraRotation(InYaw, InPitch, bInstant);
	SetCameraDistance(InDistance, bInstant);
}

void ACameraManagerBase::DoCameraRotationAndDistance(float InYaw, float InPitch, float InDistance, float InDuration, EEaseType InEaseType, bool bForce)
{
	DoCameraRotation(InYaw, InPitch, InDuration, InEaseType, bForce);
	DoCameraDistance(InDistance, InDuration, InEaseType, bForce);
}

void ACameraManagerBase::SetCameraTransform(FVector InLocation, float InYaw, float InPitch, float InDistance, bool bInstant)
{
	SetCameraLocation(InLocation, bInstant);
	SetCameraRotation(InYaw, InPitch, bInstant);
	SetCameraDistance(InDistance, bInstant);
}

void ACameraManagerBase::DoCameraTransform(FVector InLocation, float InYaw, float InPitch, float InDistance, float InDuration, EEaseType InEaseType, bool bForce)
{
	DoCameraLocation(InLocation, InDuration, InEaseType, bForce);
	DoCameraRotation(InYaw, InPitch, InDuration, InEaseType, bForce);
	DoCameraDistance(InDistance, InDuration, InEaseType, bForce);
}

void ACameraManagerBase::StopDoCameraTransform()
{
	StopDoCameraLocation();
	StopDoCameraRotation();
	StopDoCameraDistance();
}

void ACameraManagerBase::SetCameraFov(float InFov, bool bInstant)
{
	if(!CurrentCamera) return;

	TargetCameraFov = InFov != -1.f ? InFov : InitCameraFov;
	if(bInstant)
	{
		CurrentCameraFov = InFov;
		CurrentCamera->GetCamera()->SetFieldOfView(TargetCameraFov);
	}
	StopDoCameraFov();
}

void ACameraManagerBase::DoCameraFov(float InFov, float InDuration, EEaseType InEaseType, bool bForce)
{
	if(!CurrentCamera || (CameraDoFovFov != EMPTY_Flt || CurrentCameraFov == InFov)  && !bForce) return;

	TargetCameraFov = InFov != -1.f ? InFov : InitCameraFov;
	if(InDuration > 0.f)
	{
		CameraDoFovTime = 0.f;
		CameraDoFovDuration = InDuration;
		CameraDoFovFov = CurrentCameraFov;
		CameraDoFovEaseType = InEaseType;
	}
	else
	{
		CurrentCameraFov = TargetCameraFov;
		CurrentCamera->GetCamera()->SetFieldOfView(TargetCameraFov);
	}
}

void ACameraManagerBase::StopDoCameraFov()
{
	CameraDoFovTime = 0.f;
	CameraDoFovDuration = 0.f;
	CameraDoFovFov = EMPTY_Flt;
}

void ACameraManagerBase::AddCameraMovementInput(FVector InDirection, float InValue)
{
	if(UCameraModule::Get().GetModuleState() != EModuleState::Running || !UCameraModule::Get().IsCameraControlAble() || !UCameraModule::Get().IsCameraMoveControlAble() || (IsTrackingTarget() && !ENUMWITH(TrackControlMode, ECameraControlMode::LocationOnly))) return;

	SetCameraLocation(TargetCameraLocation + InDirection * InValue * UCameraModule::Get().GetCameraMoveRate() * (1.f + (UCameraModule::Get().GetCameraMoveAltitude() != 0.f ? (UCommonStatics::GetPossessedPawn() ? 0.f : FMath::Abs(USceneModuleStatics::GetAltitude(false, true)) / UCameraModule::Get().GetCameraMoveAltitude()) : 0.f)) * GetWorld()->GetDeltaSeconds(), false);
}

void ACameraManagerBase::AddCameraRotationInput(float InYaw, float InPitch)
{
	if(UCameraModule::Get().GetModuleState() != EModuleState::Running || !UCameraModule::Get().IsCameraControlAble() || !UCameraModule::Get().IsCameraRotateControlAble() || (IsTrackingTarget() && !ENUMWITH(TrackControlMode, ECameraControlMode::RotationOnly))) return;

	SetCameraRotation(TargetCameraRotation.Yaw + InYaw * UCameraModule::Get().GetCameraTurnRate() * GetWorld()->GetDeltaSeconds(), TargetCameraRotation.Pitch + InPitch * UCameraModule::Get().GetCameraLookUpRate() * GetWorld()->GetDeltaSeconds(), false);
}

void ACameraManagerBase::AddCameraDistanceInput(float InValue)
{
	if(UCameraModule::Get().GetModuleState() != EModuleState::Running || !UCameraModule::Get().IsCameraControlAble() || !UCameraModule::Get().IsCameraZoomControlAble() || (IsTrackingTarget() && !ENUMWITH(TrackControlMode, ECameraControlMode::DistanceOnly))) return;

	SetCameraDistance(TargetCameraDistance + InValue * UCameraModule::Get().GetCameraZoomRate() * (2.f + (UCameraModule::Get().GetCameraZoomAltitude() != 0.f ? (UCommonStatics::GetPossessedPawn() ? 0.f : FMath::Abs(FMath::Max(USceneModuleStatics::GetAltitude(false, true), CurrentCameraDistance)) / UCameraModule::Get().GetCameraZoomAltitude()) : 0.f)) * GetWorld()->GetDeltaSeconds(), false);

	if(UCameraModule::Get().IsCameraZoomMoveAble() && !IsTrackingTarget() && InValue < 0.f && TargetCameraDistance == 0.f)
	{
		AddCameraMovementInput(CurrentCameraRotation.Vector(), -InValue);
	}
}

void ACameraManagerBase::SetCameraView(const FCameraViewData& InCameraViewData, bool bCacheData, bool bInstant)
{
	EndTrackTarget();

	if(InCameraViewData.IsValid())
	{
		if(InCameraViewData.bTrackTarget)
		{
			if(ACameraActorBase* CameraActor = InCameraViewData.CameraViewParams.CameraViewActor.LoadSynchronous())
			{
				SwitchCamera(CameraActor);
			}
			if(InCameraViewData.CameraViewTarget.LoadSynchronous())
			{
				StartTrackTarget(InCameraViewData.CameraViewTarget.LoadSynchronous(), InCameraViewData.TrackTargetMode, InCameraViewData.CameraViewParams.CameraViewMode, InCameraViewData.CameraViewParams.CameraViewSpace,
					InCameraViewData.CameraViewParams.CameraViewLocation, FVector(-1.f), InCameraViewData.CameraViewParams.CameraViewYaw,
					InCameraViewData.CameraViewParams.CameraViewPitch, InCameraViewData.CameraViewParams.CameraViewDistance, true, InCameraViewData.CameraViewParams.CameraViewEaseType, InCameraViewData.CameraViewParams.CameraViewDuration, bInstant);
			}
			if(bCacheData)
			{
				const ECameraTrackMode TrackTargetMode = InCameraViewData.TrackTargetMode;
				CachedCameraViewData = TrackCameraViewData;
				CachedCameraViewData.TrackTargetMode = TrackTargetMode;
			}
		}
		else
		{
			SetCameraViewParams(InCameraViewData.CameraViewParams, bInstant);
			if(bCacheData)
			{
				CachedCameraViewData = InCameraViewData;
			}
		}
	}
	else if(bCacheData)
	{
		CachedCameraViewData = InCameraViewData;
	}
}

void ACameraManagerBase::SetCameraViewParams(const FCameraViewParams& InCameraViewParams, bool bInstant)
{
	if(!InCameraViewParams.IsValid()) return;
	
	if(ACameraActorBase* CameraActor = InCameraViewParams.CameraViewActor.LoadSynchronous())
	{
		SwitchCamera(CameraActor);
	}

	const FVector CameraLocation = InCameraViewParams.GetCameraLocation(true);
	const FVector CameraOffset = InCameraViewParams.GetCameraOffset();
	const float CameraYaw = InCameraViewParams.GetCameraYaw();
	const float CameraPitch = InCameraViewParams.GetCameraPitch();
	const float CameraDistance = InCameraViewParams.GetCameraDistance();
	const float CameraFov = InCameraViewParams.GetCameraFov();

	switch(InCameraViewParams.CameraViewMode)
	{
		case ECameraViewMode::Instant:
		{
			SetCameraLocation(CameraLocation, true);
			SetCameraOffset(CameraOffset, true);
			SetCameraRotation(CameraYaw, CameraPitch, true);
			SetCameraDistance(CameraDistance, true);
			SetCameraFov(CameraFov, true);
			break;
		}
		case ECameraViewMode::Smooth:
		{
			SetCameraLocation(CameraLocation, bInstant);
			SetCameraOffset(CameraOffset, bInstant);
			SetCameraRotation(CameraYaw, CameraPitch, bInstant);
			SetCameraDistance(CameraDistance, bInstant);
			SetCameraFov(CameraFov, bInstant);
			break;
		}
		case ECameraViewMode::Duration:
		{
			DoCameraLocation(CameraLocation, bInstant ? 0.f : InCameraViewParams.CameraViewDuration, InCameraViewParams.CameraViewEaseType);
			DoCameraOffset(CameraOffset, bInstant ? 0.f : InCameraViewParams.CameraViewDuration, InCameraViewParams.CameraViewEaseType);
			DoCameraRotation(CameraYaw, CameraPitch, bInstant ? 0.f : InCameraViewParams.CameraViewDuration, InCameraViewParams.CameraViewEaseType);
			DoCameraDistance(CameraDistance, bInstant ? 0.f : InCameraViewParams.CameraViewDuration, InCameraViewParams.CameraViewEaseType);
			DoCameraFov(CameraFov, bInstant ? 0.f : InCameraViewParams.CameraViewDuration, InCameraViewParams.CameraViewEaseType);
			break;
		}
		default: break;
	}
}

void ACameraManagerBase::ResetCameraView(ECameraResetMode InCameraResetMode, bool bInstant)
{
	switch(InCameraResetMode)
	{
		case ECameraResetMode::DefaultPoint:
		{
			if(DefaultCameraPoint)
			{
				SwitchCameraPoint(DefaultCameraPoint);
			}
			break;
		}
		case ECameraResetMode::CurrentPoint:
		{
			if(CurrentCameraPoint)
			{
				SwitchCameraPoint(CurrentCameraPoint);
			}
			break;
		}
		case ECameraResetMode::CachedData:
		{
			if(CachedCameraViewData.IsValid())
			{
				SetCameraView(CachedCameraViewData, false);
			}
			break;
		}
	}
}

bool ACameraManagerBase::IsControllingMove()
{
	return UInputModuleStatics::GetKeyShortcut(GameplayTags::Input_CameraPanMove).IsPressing(PCOwner) || UInputModuleStatics::GetTouchPressedCount() == 3;
}

bool ACameraManagerBase::IsControllingRotate()
{
	return UInputModuleStatics::GetKeyShortcut(GameplayTags::Input_CameraRotate).IsPressing(PCOwner) || UInputModuleStatics::GetTouchPressedCount() == 1;
}

bool ACameraManagerBase::IsControllingZoom()
{
	return UInputModuleStatics::GetKeyShortcut(GameplayTags::Input_CameraZoom).IsPressing(PCOwner) || UInputModuleStatics::GetTouchPressedCount() == 2;
}

bool ACameraManagerBase::IsTrackingTarget() const
{
	return TrackCameraViewData.CameraViewParams.CameraViewTarget != nullptr;
}

float ACameraManagerBase::GetMinCameraPitch() const
{
	return GetTrackingTarget() && GetTrackingTarget()->Implements<UCameraTrackableInterface>() && ICameraTrackableInterface::Execute_GetCameraMinPitch(GetTrackingTarget()) != -1.f ? ICameraTrackableInterface::Execute_GetCameraMinPitch(GetTrackingTarget()) : UCameraModule::Get().GetMinCameraPitch();
}

float ACameraManagerBase::GetMaxCameraPitch() const
{
	return GetTrackingTarget() && GetTrackingTarget()->Implements<UCameraTrackableInterface>() && ICameraTrackableInterface::Execute_GetCameraMaxPitch(GetTrackingTarget()) != -1.f ? ICameraTrackableInterface::Execute_GetCameraMaxPitch(GetTrackingTarget()) : UCameraModule::Get().GetMaxCameraPitch();
}

FVector ACameraManagerBase::GetRealCameraLocation()
{
	return GetCameraLocation();
}

FVector ACameraManagerBase::GetCurrentCameraLocation(bool bRefresh) const
{
	if(bRefresh)
	{
		return CurrentCamera ? CurrentCamera->GetActorLocation() : FVector::ZeroVector;
	}
	return CurrentCameraLocation;
}

FVector ACameraManagerBase::GetRealCameraOffset() const
{
	if(CurrentCamera && CurrentCamera->GetCameraBoom())
	{
		return CurrentCamera->GetCameraBoom()->SocketOffset;
	}
	return FVector::ZeroVector;
}

FVector ACameraManagerBase::GetCurrentCameraOffset(bool bRefresh) const
{
	if(bRefresh)
	{
		return CurrentCamera ? CurrentCamera->GetCameraBoom()->SocketOffset : FVector::ZeroVector;
	}
	return CurrentCameraOffset;
}

FRotator ACameraManagerBase::GetRealCameraRotation()
{
	return GetCameraRotation();
}

FRotator ACameraManagerBase::GetCurrentCameraRotation(bool bRefresh)
{
	if(bRefresh)
	{
		return PCOwner ? PCOwner->GetControlRotation() : FRotator::ZeroRotator;
	}
	return CurrentCameraRotation;
}

float ACameraManagerBase::GetRealCameraDistance() const
{
	if(CurrentCamera && CurrentCamera->GetCameraBoom())
	{
		return CurrentCamera->GetCameraBoom()->TargetArmLength;
	}
	return 0.f;
}

float ACameraManagerBase::GetCurrentCameraDistance(bool bRefresh) const
{
	if(bRefresh)
	{
		return CurrentCamera ? CurrentCamera->GetCameraBoom()->TargetArmLength : 0;
	}
	return CurrentCameraDistance;
}

float ACameraManagerBase::GetRealCameraFov() const
{
	if(CurrentCamera && CurrentCamera->GetCameraBoom())
	{
		return CurrentCamera->GetCamera()->FieldOfView;
	}
	return 0.f;
}

float ACameraManagerBase::GetCurrentCameraFov(bool bRefresh) const
{
	if(bRefresh)
	{
		return CurrentCamera ? CurrentCamera->GetCamera()->FieldOfView : 0;
	}
	return CurrentCameraFov;
}

void ACameraManagerBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

}

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Camera/Manager/CameraManagerBase.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraModule.h"
#include "Camera/Actor/CameraActorBase.h"
#include "Camera/Interface/CameraTrackableInterface.h"
#include "Common/CommonModuleStatics.h"
#include "Event/EventModuleStatics.h"
#include "Event/Events/Camera/Event_CameraPointChanged.h"
#include "GameFramework/SpringArmComponent.h"
#include "Math/MathHelper.h"
#include "Scene/SceneModuleStatics.h"
#include "Camera/Point/CameraPointBase.h"
#include "Event/Events/Camera/Event_CameraTraceEnded.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
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

	LocalPlayerIndex = INDEX_NONE;
	bOwnsRuntimeCameras = false;
	RuntimeState = FCameraRuntimeState();
	TrackProfile = FCameraTrackProfile();
}

void ACameraManagerBase::InitializeFor(APlayerController* PC)
{
	Super::InitializeFor(PC);

	if(!PC || !PC->GetLocalPlayer() || !GetWorld() || !GetWorld()->GetGameInstance())
	{
		return;
	}

	LocalPlayerIndex = GetWorld()->GetGameInstance()->GetLocalPlayers().IndexOfByKey(PC->GetLocalPlayer());
	if(LocalPlayerIndex == INDEX_NONE)
	{
		return;
	}

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

	DeltaTime /= UCommonModuleStatics::GetTimeScale();

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
				CurrentCamera->SetCameraLocation(!RuntimeSettings.bSmoothMove ? TargetCameraLocation : FMath::VInterpTo(CurrentCameraLocation, TargetCameraLocation, DeltaTime, RuntimeSettings.MoveSpeed));
			}
			CurrentCameraLocation = CurrentCamera->GetActorLocation();
		}
		else if(CameraDoLocationDuration != 0.f)
		{
			StopTransitionLocationInternal();
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
			StopTransitionOffsetInternal();
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
				PCOwner->SetControlRotation(!RuntimeSettings.bSmoothRotate ? TargetCameraRotation : FMath::RInterpTo(CurrentCameraRotation, TargetCameraRotation, DeltaTime, RuntimeSettings.RotateSpeed));
			}
			CurrentCameraRotation = PCOwner->GetControlRotation();
		}
		else if(CameraDoRotationDuration != 0.f)
		{
			StopTransitionRotationInternal();
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
				CurrentCamera->GetCameraBoom()->TargetArmLength = !RuntimeSettings.bSmoothZoom ? TargetDistance : FMath::FInterpTo(CurrentCameraDistance, TargetDistance, DeltaTime, UCameraModule::Get().IsNormalizeCameraZoom() && UCameraModule::Get().GetMaxCameraDistance() != -1.f ? UKismetMathLibrary::NormalizeToRange(CurrentCamera->GetCameraBoom()->TargetArmLength, UCameraModule::Get().GetMinCameraDistance(), UCameraModule::Get().GetMaxCameraDistance()) * RuntimeSettings.ZoomSpeed : RuntimeSettings.ZoomSpeed);
			}
			CurrentCameraDistance = CurrentCamera->GetCameraBoom()->TargetArmLength;
		}
		else if(CameraDoDistanceDuration != 0.f)
		{
			StopTransitionDistanceInternal();
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
			StopTransitionFovInternal();
		}
	}

	if(CurrentCamera && CurrentCamera->GetCamera()->ProjectionMode == ECameraProjectionMode::Orthographic)
	{
		CurrentCamera->GetCamera()->SetOrthoWidth(USceneModuleStatics::GetAltitude(false, true) * CurrentCamera->GetCameraOrthoFactor());
	}

	RuntimeState.Camera = CurrentCamera;
	RuntimeState.Location = CurrentCameraLocation;
	RuntimeState.Offset = CurrentCameraOffset;
	RuntimeState.Rotation = CurrentCameraRotation;
	RuntimeState.Distance = CurrentCameraDistance;
	RuntimeState.FOV = CurrentCameraFov;
	RuntimeState.bTransitioning = CameraDoLocationDuration != 0.f || CameraDoOffsetDuration != 0.f || CameraDoRotationDuration != 0.f || CameraDoDistanceDuration != 0.f || CameraDoFovDuration != 0.f;
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
		RuntimeState.Camera = CurrentCamera;
		CurrentCamera->OnSwitch();
		if(PCOwner)
		{
			PCOwner->SetViewTarget(InCamera);
		}
		if(bReset)
		{
			ApplyLocationInternal(InCamera->GetActorLocation(), bInstant);
			ApplyRotationInternal(InCamera->GetActorRotation().Yaw, InCamera->GetActorRotation().Pitch, bInstant);
			ApplyDistanceInternal(InCamera->GetCameraBoom()->TargetArmLength, bInstant);
			ApplyOffsetInternal(InitCameraOffset = CurrentCamera->GetCameraBoom()->SocketOffset, bInstant);
			ApplyFovInternal(InitCameraFov = CurrentCamera->GetCamera()->FieldOfView, bInstant);
		}
		else
		{
			ApplyLocationInternal(CurrentCameraLocation, bInstant);
			ApplyRotationInternal(CurrentCameraRotation.Yaw, CurrentCameraRotation.Pitch, bInstant);
			ApplyDistanceInternal(CurrentCameraDistance, bInstant);
			ApplyOffsetInternal(CurrentCameraOffset, bInstant);
			ApplyFovInternal(CurrentCameraFov, bInstant);
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
		RuntimeState.Camera = nullptr;
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
	if(!InCameraPoint) return;

	if(CurrentCameraPoint != InCameraPoint)
	{
		CurrentCameraPoint = InCameraPoint;
		RuntimeState.CameraPoint = CurrentCameraPoint;
		UEventModuleStatics::BroadcastEvent<FEventCameraPointChanged>(this, { CurrentCameraPoint });
	}
	ApplyViewData(InCameraPoint->GetCameraViewData(), true, bInstant);
	if(bSetAsDefault)
	{
		SetDefaultCameraPoint(InCameraPoint);
	}
}

void ACameraManagerBase::DoTrackTarget(bool bInstant)
{
	if(!TrackTarget.IsValid())
	{
		if(RuntimeState.bTracking)
		{
			ClearTarget();
		}
		return;
	}
	
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
				ApplyLocationInternal(TrackCameraViewData.CameraViewParams.CameraViewTarget->GetActorLocation() + TrackCameraViewData.CameraViewParams.CameraViewTarget->GetActorRotation().RotateVector(TrackCameraViewData.CameraViewParams.CameraViewLocation), true);
				break;
			}
			case ECameraViewMode::Smooth:
			{
				ApplyLocationInternal(TrackCameraViewData.CameraViewParams.CameraViewTarget->GetActorLocation() + TrackCameraViewData.CameraViewParams.CameraViewTarget->GetActorRotation().RotateVector(TrackCameraViewData.CameraViewParams.CameraViewLocation), !ENUMWITH(TrackSmoothMode, ECameraSmoothMode::LocationOnly) || TrackCameraViewData.CameraViewParams.CameraViewMode == ECameraViewMode::Instant || bInstant);
				break;
			}
			case ECameraViewMode::Duration:
			{
				TransitionLocationInternal(TrackCameraViewData.CameraViewParams.CameraViewTarget->GetActorLocation() + TrackCameraViewData.CameraViewParams.CameraViewTarget->GetActorRotation().RotateVector(TrackCameraViewData.CameraViewParams.CameraViewLocation), bInstant ? 0.f : TrackCameraViewData.CameraViewParams.CameraViewDuration, TrackCameraViewData.CameraViewParams.CameraViewEaseType, false);
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
				ApplyRotationInternal(TrackCameraViewData.CameraViewParams.CameraViewTarget->GetActorRotation().Yaw + TrackCameraViewData.CameraViewParams.CameraViewYaw, TrackCameraViewData.CameraViewParams.CameraViewTarget->GetActorRotation().Pitch + TrackCameraViewData.CameraViewParams.CameraViewPitch, true);
				break;
			}
			case ECameraViewMode::Smooth:
			{
				ApplyRotationInternal(TrackCameraViewData.CameraViewParams.CameraViewTarget->GetActorRotation().Yaw + TrackCameraViewData.CameraViewParams.CameraViewYaw, TrackCameraViewData.CameraViewParams.CameraViewTarget->GetActorRotation().Pitch + TrackCameraViewData.CameraViewParams.CameraViewPitch, !ENUMWITH(TrackSmoothMode, ECameraSmoothMode::RotationOnly) || TrackCameraViewData.CameraViewParams.CameraViewMode == ECameraViewMode::Instant || bInstant);
				break;
			}
			case ECameraViewMode::Duration:
			{
				TransitionRotationInternal(TrackCameraViewData.CameraViewParams.CameraViewTarget->GetActorRotation().Yaw + TrackCameraViewData.CameraViewParams.CameraViewYaw, TrackCameraViewData.CameraViewParams.CameraViewTarget->GetActorRotation().Pitch + TrackCameraViewData.CameraViewParams.CameraViewPitch, bInstant ? 0.f : TrackCameraViewData.CameraViewParams.CameraViewDuration, TrackCameraViewData.CameraViewParams.CameraViewEaseType, false);
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
				ApplyDistanceInternal(TrackCameraViewData.CameraViewParams.CameraViewDistance, true);
				break;
			}
			case ECameraViewMode::Smooth:
			{
				ApplyDistanceInternal(TrackCameraViewData.CameraViewParams.CameraViewDistance, !ENUMWITH(TrackSmoothMode, ECameraSmoothMode::DistanceOnly) || TrackCameraViewData.CameraViewParams.CameraViewMode == ECameraViewMode::Instant || bInstant);
				break;
			}
			case ECameraViewMode::Duration:
			{
				TransitionDistanceInternal(TrackCameraViewData.CameraViewParams.CameraViewDistance, bInstant ? 0.f : TrackCameraViewData.CameraViewParams.CameraViewDuration, TrackCameraViewData.CameraViewParams.CameraViewEaseType, false);
				break;
			}
			default: break;
		}
	}
}

void ACameraManagerBase::BindTarget(const FCameraTargetRequest& InRequest)
{
	if(!InRequest.Target)
	{
		ClearTarget();
		return;
	}

	ClearTarget();
	TrackProfile = InRequest.Profile;
	TrackSmoothMode = TrackProfile.SmoothMode;
	TrackControlMode = TrackProfile.ControlMode;
	TrackTarget = InRequest.Target;
	TrackCameraViewData.CameraViewTarget = InRequest.Target;
	TrackCameraViewData.bTrackTarget = true;
	TrackCameraViewData.TrackTargetMode = TrackProfile.TrackMode;
	TrackCameraViewData.CameraViewParams.CameraViewTarget = InRequest.Target;
	TrackCameraViewData.CameraViewParams.CameraViewMode = InRequest.ViewMode;
	TrackCameraViewData.CameraViewParams.CameraViewSpace = InRequest.ViewSpace;
	TrackCameraViewData.CameraViewParams.CameraViewLocation = FVector::ZeroVector;
	TrackCameraViewData.CameraViewParams.CameraViewOffset = TargetCameraOffset = TrackProfile.Offset;
	TrackCameraViewData.CameraViewParams.CameraViewYaw = InRequest.bResetRotation ? 0.f : TargetCameraRotation.Yaw - InRequest.Target->GetActorRotation().Yaw;
	TrackCameraViewData.CameraViewParams.CameraViewPitch = InRequest.bResetRotation ? 0.f : TargetCameraRotation.Pitch - InRequest.Target->GetActorRotation().Pitch;
	TrackCameraViewData.CameraViewParams.CameraViewDistance = TrackProfile.Distance;
	bTrackAllowControl = InRequest.bAllowControl;
	RuntimeState.Target = InRequest.Target;
	RuntimeState.bTracking = true;
	DoTrackTarget(InRequest.bInstant);
}

void ACameraManagerBase::ClearTarget(AActor* InExpectedTarget)
{
	AActor* CurrentTarget = TrackTarget.Get();
	if(InExpectedTarget && CurrentTarget != InExpectedTarget) return;

	if(CurrentTarget)
	{
		UEventModuleStatics::BroadcastEvent<FEventCameraTraceEnded>(this, { CurrentTarget });
	}

	TrackTarget.Reset();
	TrackCameraViewData = FCameraViewData();
	TrackProfile = FCameraTrackProfile();
	bTrackAllowControl = false;
	TrackSmoothMode = ECameraSmoothMode::None;
	TrackControlMode = ECameraControlMode::None;
	RuntimeState.Target = nullptr;
	RuntimeState.bTracking = false;

	if(CurrentCamera)
	{
		ApplyOffsetInternal(InitCameraOffset);
	}
}

void ACameraManagerBase::ApplyLocationInternal(FVector InLocation, bool bInstant)
{
	if(!CurrentCamera) return;

	TargetCameraLocation = UCameraModule::Get().GetCameraMoveRange().IsValid && !IsTrackingTarget() ? ClampVector(InLocation, UCameraModule::Get().GetCameraMoveRange().Min, UCameraModule::Get().GetCameraMoveRange().Max) : InLocation;
	if(bInstant)
	{
		CurrentCameraLocation = TargetCameraLocation;
		CurrentCamera->SetCameraLocation(TargetCameraLocation);
	}
	StopTransitionLocationInternal();
}

void ACameraManagerBase::TransitionLocationInternal(FVector InLocation, float InDuration, EEaseType InEaseType, bool bForce)
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
	RefreshTransitionState();
}

void ACameraManagerBase::StopTransitionLocationInternal()
{
	CameraDoLocationTime = 0.f;
	CameraDoLocationDuration = 0.f;
	CameraDoLocationLocation = EMPTY_Vector;
	RefreshTransitionState();
}

void ACameraManagerBase::ApplyOffsetInternal(FVector InOffset, bool bInstant)
{
	if(!CurrentCamera) return;

	InOffset = InOffset != FVector(-1.f) ? InOffset : (IsTrackingTarget() ? TrackCameraViewData.CameraViewParams.CameraViewOffset : InitCameraOffset);

	TargetCameraOffset = UCameraModule::Get().GetCameraMoveRange().IsValid && !IsTrackingTarget() ? ClampVector(InOffset, UCameraModule::Get().GetCameraMoveRange().Min, UCameraModule::Get().GetCameraMoveRange().Max) : InOffset;
	if(bInstant)
	{
		CurrentCameraOffset = TargetCameraOffset;
		CurrentCamera->GetCameraBoom()->SocketOffset = TargetCameraOffset;
	}
	StopTransitionOffsetInternal();
}

void ACameraManagerBase::TransitionOffsetInternal(FVector InOffset, float InDuration, EEaseType InEaseType, bool bForce)
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
	RefreshTransitionState();
}

void ACameraManagerBase::StopTransitionOffsetInternal()
{
	CameraDoOffsetTime = 0.f;
	CameraDoOffsetDuration = 0.f;
	CameraDoOffsetOffset = EMPTY_Vector;
	RefreshTransitionState();
}

void ACameraManagerBase::ApplyRotationInternal(float InYaw, float InPitch, bool bInstant)
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
	StopTransitionRotationInternal();
}

void ACameraManagerBase::TransitionRotationInternal(float InYaw, float InPitch, float InDuration, EEaseType InEaseType, bool bForce)
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
	RefreshTransitionState();
}

void ACameraManagerBase::StopTransitionRotationInternal()
{
	CameraDoRotationTime = 0.f;
	CameraDoRotationDuration = 0.f;
	CameraDoRotationRotation = EMPTY_Rotator;
	RefreshTransitionState();
}

void ACameraManagerBase::ApplyDistanceInternal(float InDistance, bool bInstant)
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
	StopTransitionDistanceInternal();
}

void ACameraManagerBase::TransitionDistanceInternal(float InDistance, float InDuration, EEaseType InEaseType, bool bForce)
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
	RefreshTransitionState();
}

void ACameraManagerBase::StopTransitionDistanceInternal()
{
	CameraDoDistanceTime = 0.f;
	CameraDoDistanceDuration = 0.f;
	CameraDoDistanceDistance = EMPTY_Flt;
	RefreshTransitionState();
}

void ACameraManagerBase::ApplyRotationAndDistanceInternal(float InYaw, float InPitch, float InDistance, bool bInstant)
{
	ApplyRotationInternal(InYaw, InPitch, bInstant);
	ApplyDistanceInternal(InDistance, bInstant);
}

void ACameraManagerBase::TransitionRotationAndDistanceInternal(float InYaw, float InPitch, float InDistance, float InDuration, EEaseType InEaseType, bool bForce)
{
	TransitionRotationInternal(InYaw, InPitch, InDuration, InEaseType, bForce);
	TransitionDistanceInternal(InDistance, InDuration, InEaseType, bForce);
}

void ACameraManagerBase::ApplyTransformInternal(FVector InLocation, float InYaw, float InPitch, float InDistance, bool bInstant)
{
	ApplyLocationInternal(InLocation, bInstant);
	ApplyRotationInternal(InYaw, InPitch, bInstant);
	ApplyDistanceInternal(InDistance, bInstant);
}

void ACameraManagerBase::TransitionTransformInternal(FVector InLocation, float InYaw, float InPitch, float InDistance, float InDuration, EEaseType InEaseType, bool bForce)
{
	TransitionLocationInternal(InLocation, InDuration, InEaseType, bForce);
	TransitionRotationInternal(InYaw, InPitch, InDuration, InEaseType, bForce);
	TransitionDistanceInternal(InDistance, InDuration, InEaseType, bForce);
}

void ACameraManagerBase::StopTransitionTransformInternal()
{
	StopTransitionLocationInternal();
	StopTransitionRotationInternal();
	StopTransitionDistanceInternal();
}

void ACameraManagerBase::ApplyFovInternal(float InFov, bool bInstant)
{
	if(!CurrentCamera) return;

	TargetCameraFov = InFov != -1.f ? InFov : InitCameraFov;
	if(bInstant)
	{
		CurrentCameraFov = TargetCameraFov;
		CurrentCamera->GetCamera()->SetFieldOfView(TargetCameraFov);
	}
	StopTransitionFovInternal();
}

void ACameraManagerBase::TransitionFovInternal(float InFov, float InDuration, EEaseType InEaseType, bool bForce)
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
	RefreshTransitionState();
}

void ACameraManagerBase::StopTransitionFovInternal()
{
	CameraDoFovTime = 0.f;
	CameraDoFovDuration = 0.f;
	CameraDoFovFov = EMPTY_Flt;
	RefreshTransitionState();
}

void ACameraManagerBase::RefreshTransitionState()
{
	RuntimeState.bTransitioning = CameraDoLocationDuration != 0.f
		|| CameraDoOffsetDuration != 0.f
		|| CameraDoRotationDuration != 0.f
		|| CameraDoDistanceDuration != 0.f
		|| CameraDoFovDuration != 0.f;
}

void ACameraManagerBase::ApplyMoveInputInternal(FVector InDirection, float InValue)
{
	if(UCameraModule::Get().GetModuleState() != EModuleState::Running || !UCameraModule::Get().IsCameraControlAble() || !UCameraModule::Get().IsCameraMoveControlAble() || (IsTrackingTarget() && !ENUMWITH(TrackControlMode, ECameraControlMode::LocationOnly))) return;

	ApplyLocationInternal(TargetCameraLocation + InDirection * InValue * RuntimeSettings.MoveRate * (1.f + (UCameraModule::Get().GetCameraMoveAltitude() != 0.f ? (UCommonModuleStatics::GetPossessedPawn() ? 0.f : FMath::Abs(USceneModuleStatics::GetAltitude(false, true)) / UCameraModule::Get().GetCameraMoveAltitude()) : 0.f)) * GetWorld()->GetDeltaSeconds(), false);
}

void ACameraManagerBase::ApplyLookInputInternal(float InYaw, float InPitch)
{
	if(UCameraModule::Get().GetModuleState() != EModuleState::Running || !UCameraModule::Get().IsCameraControlAble() || !UCameraModule::Get().IsCameraRotateControlAble() || (IsTrackingTarget() && !ENUMWITH(TrackControlMode, ECameraControlMode::RotationOnly))) return;

	ApplyRotationInternal(TargetCameraRotation.Yaw + InYaw * RuntimeSettings.TurnRate * GetWorld()->GetDeltaSeconds(), TargetCameraRotation.Pitch + InPitch * RuntimeSettings.LookUpRate * GetWorld()->GetDeltaSeconds(), false);
}

void ACameraManagerBase::ApplyZoomInputInternal(float InValue)
{
	if(UCameraModule::Get().GetModuleState() != EModuleState::Running || !UCameraModule::Get().IsCameraControlAble() || !UCameraModule::Get().IsCameraZoomControlAble() || (IsTrackingTarget() && !ENUMWITH(TrackControlMode, ECameraControlMode::DistanceOnly))) return;

	ApplyDistanceInternal(TargetCameraDistance + InValue * RuntimeSettings.ZoomRate * (2.f + (UCameraModule::Get().GetCameraZoomAltitude() != 0.f ? (UCommonModuleStatics::GetPossessedPawn() ? 0.f : FMath::Abs(FMath::Max(USceneModuleStatics::GetAltitude(false, true), CurrentCameraDistance)) / UCameraModule::Get().GetCameraZoomAltitude()) : 0.f)) * GetWorld()->GetDeltaSeconds(), false);

	if(UCameraModule::Get().IsCameraZoomMoveAble() && !IsTrackingTarget() && InValue < 0.f && TargetCameraDistance == 0.f)
	{
		ApplyMoveInputInternal(CurrentCameraRotation.Vector(), -InValue);
	}
}

void ACameraManagerBase::AddLookInput(const FVector2D& InValue)
{
	FVector2D Value = InValue;
	if(RuntimeSettings.bReversePitch)
	{
		Value.Y *= -1.f;
	}
	ApplyLookInputInternal(Value.X, Value.Y);
}

void ACameraManagerBase::AddPanInput(const FVector2D& InValue)
{
	if(!PCOwner) return;

	constexpr float CameraPanScale = 0.7f;
	const float PanScale = RuntimeSettings.bReversePanMove ? -CameraPanScale : CameraPanScale;
	const FRotator HorizontalRotation(0.f, PCOwner->GetControlRotation().Yaw, 0.f);
	const FRotator VerticalRotation(RuntimeSettings.bEnablePanZMove ? PCOwner->GetControlRotation().Pitch : 0.f, PCOwner->GetControlRotation().Yaw, 0.f);
	const FVector HorizontalDirection = FRotationMatrix(HorizontalRotation).GetUnitAxis(EAxis::Y);
	const FVector VerticalDirection = FRotationMatrix(VerticalRotation).GetUnitAxis(RuntimeSettings.bEnablePanZMove ? EAxis::Z : EAxis::X);
	ApplyMoveInputInternal(HorizontalDirection * PanScale, InValue.X);
	ApplyMoveInputInternal(VerticalDirection * PanScale, InValue.Y);
}

void ACameraManagerBase::AddMoveInput(const FVector& InValue)
{
	ApplyMoveInputInternal(InValue, 1.f);
}

void ACameraManagerBase::AddZoomInput(float InValue)
{
	ApplyZoomInputInternal(InValue);
}

void ACameraManagerBase::ApplyView(const FCameraViewRequest& InRequest)
{
	const ECameraViewProperty Properties = static_cast<ECameraViewProperty>(InRequest.Properties);
	const bool bInstant = InRequest.Transition.Mode == ECameraViewMode::Instant;
	const bool bDuration = InRequest.Transition.Mode == ECameraViewMode::Duration;

	if(EnumHasAnyFlags(Properties, ECameraViewProperty::Location))
	{
		bDuration ? TransitionLocationInternal(InRequest.Location, InRequest.Transition.Duration, InRequest.Transition.EaseType, InRequest.Transition.bForce) : ApplyLocationInternal(InRequest.Location, bInstant);
	}
	if(EnumHasAnyFlags(Properties, ECameraViewProperty::Offset))
	{
		bDuration ? TransitionOffsetInternal(InRequest.Offset, InRequest.Transition.Duration, InRequest.Transition.EaseType, InRequest.Transition.bForce) : ApplyOffsetInternal(InRequest.Offset, bInstant);
	}
	if(EnumHasAnyFlags(Properties, ECameraViewProperty::Rotation))
	{
		bDuration ? TransitionRotationInternal(InRequest.Rotation.Yaw, InRequest.Rotation.Pitch, InRequest.Transition.Duration, InRequest.Transition.EaseType, InRequest.Transition.bForce) : ApplyRotationInternal(InRequest.Rotation.Yaw, InRequest.Rotation.Pitch, bInstant);
	}
	if(EnumHasAnyFlags(Properties, ECameraViewProperty::Distance))
	{
		bDuration ? TransitionDistanceInternal(InRequest.Distance, InRequest.Transition.Duration, InRequest.Transition.EaseType, InRequest.Transition.bForce) : ApplyDistanceInternal(InRequest.Distance, bInstant);
	}
	if(EnumHasAnyFlags(Properties, ECameraViewProperty::FOV))
	{
		bDuration ? TransitionFovInternal(InRequest.FOV, InRequest.Transition.Duration, InRequest.Transition.EaseType, InRequest.Transition.bForce) : ApplyFovInternal(InRequest.FOV, bInstant);
	}
}

void ACameraManagerBase::StopViewTransition(int32 InProperties)
{
	const ECameraViewProperty Properties = static_cast<ECameraViewProperty>(InProperties);
	if(EnumHasAnyFlags(Properties, ECameraViewProperty::Location)) StopTransitionLocationInternal();
	if(EnumHasAnyFlags(Properties, ECameraViewProperty::Offset)) StopTransitionOffsetInternal();
	if(EnumHasAnyFlags(Properties, ECameraViewProperty::Rotation)) StopTransitionRotationInternal();
	if(EnumHasAnyFlags(Properties, ECameraViewProperty::Distance)) StopTransitionDistanceInternal();
	if(EnumHasAnyFlags(Properties, ECameraViewProperty::FOV)) StopTransitionFovInternal();
}

void ACameraManagerBase::ApplyViewData(const FCameraViewData& InCameraViewData, bool bCacheData, bool bInstant)
{
	ClearTarget();

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
				FCameraTargetRequest Request;
				Request.Target = InCameraViewData.CameraViewTarget.LoadSynchronous();
				if(Request.Target->Implements<UCameraTrackableInterface>())
				{
					Request.Profile = ICameraTrackableInterface::Execute_GetCameraTrackProfile(Request.Target);
				}
				Request.Profile.Offset = InCameraViewData.CameraViewParams.CameraViewOffset;
				Request.Profile.TrackMode = InCameraViewData.TrackTargetMode;
				Request.Profile.Distance = InCameraViewData.CameraViewParams.CameraViewDistance;
				Request.ViewMode = InCameraViewData.CameraViewParams.CameraViewMode;
				Request.ViewSpace = InCameraViewData.CameraViewParams.CameraViewSpace;
				Request.bInstant = bInstant;
				BindTarget(Request);
				TrackCameraViewData.CameraViewParams.CameraViewLocation = InCameraViewData.CameraViewParams.CameraViewLocation;
				TrackCameraViewData.CameraViewParams.CameraViewYaw = InCameraViewData.CameraViewParams.CameraViewYaw;
				TrackCameraViewData.CameraViewParams.CameraViewPitch = InCameraViewData.CameraViewParams.CameraViewPitch;
				TrackCameraViewData.CameraViewParams.CameraViewEaseType = InCameraViewData.CameraViewParams.CameraViewEaseType;
				TrackCameraViewData.CameraViewParams.CameraViewDuration = InCameraViewData.CameraViewParams.CameraViewDuration;
				DoTrackTarget(bInstant);
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
			ApplyViewParamsInternal(InCameraViewData.CameraViewParams, bInstant);
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

void ACameraManagerBase::ApplyViewParamsInternal(const FCameraViewParams& InCameraViewParams, bool bInstant)
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
			ApplyLocationInternal(CameraLocation, true);
			ApplyOffsetInternal(CameraOffset, true);
			ApplyRotationInternal(CameraYaw, CameraPitch, true);
			ApplyDistanceInternal(CameraDistance, true);
			ApplyFovInternal(CameraFov, true);
			break;
		}
		case ECameraViewMode::Smooth:
		{
			ApplyLocationInternal(CameraLocation, bInstant);
			ApplyOffsetInternal(CameraOffset, bInstant);
			ApplyRotationInternal(CameraYaw, CameraPitch, bInstant);
			ApplyDistanceInternal(CameraDistance, bInstant);
			ApplyFovInternal(CameraFov, bInstant);
			break;
		}
		case ECameraViewMode::Duration:
		{
			TransitionLocationInternal(CameraLocation, bInstant ? 0.f : InCameraViewParams.CameraViewDuration, InCameraViewParams.CameraViewEaseType);
			TransitionOffsetInternal(CameraOffset, bInstant ? 0.f : InCameraViewParams.CameraViewDuration, InCameraViewParams.CameraViewEaseType);
			TransitionRotationInternal(CameraYaw, CameraPitch, bInstant ? 0.f : InCameraViewParams.CameraViewDuration, InCameraViewParams.CameraViewEaseType);
			TransitionDistanceInternal(CameraDistance, bInstant ? 0.f : InCameraViewParams.CameraViewDuration, InCameraViewParams.CameraViewEaseType);
			TransitionFovInternal(CameraFov, bInstant ? 0.f : InCameraViewParams.CameraViewDuration, InCameraViewParams.CameraViewEaseType);
			break;
		}
		default: break;
	}
}

void ACameraManagerBase::ResetView(ECameraResetMode InCameraResetMode, bool bInstant)
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
				ApplyViewData(CachedCameraViewData, false);
			}
			break;
		}
	}
}

bool ACameraManagerBase::IsControllingMove()
{
	return UInputModuleStatics::IsInputActionActive(GameplayTags::Input_PanHCamera)
		|| UInputModuleStatics::IsInputActionActive(GameplayTags::Input_PanVCamera);
}

bool ACameraManagerBase::IsControllingRotate()
{
	return UInputModuleStatics::IsInputActionActive(GameplayTags::Input_TurnCamera)
		|| UInputModuleStatics::IsInputActionActive(GameplayTags::Input_LookUpCamera);
}

bool ACameraManagerBase::IsControllingZoom()
{
	return UInputModuleStatics::IsInputActionActive(GameplayTags::Input_CameraZoomModifier);
}

bool ACameraManagerBase::IsTrackingTarget() const
{
	return RuntimeState.bTracking;
}

float ACameraManagerBase::GetMinCameraPitch() const
{
	return IsTrackingTarget() ? TrackProfile.MinPitch : UCameraModule::Get().GetMinCameraPitch();
}

float ACameraManagerBase::GetMaxCameraPitch() const
{
	return IsTrackingTarget() ? TrackProfile.MaxPitch : UCameraModule::Get().GetMaxCameraPitch();
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

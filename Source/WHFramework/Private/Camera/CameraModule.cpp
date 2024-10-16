// Fill out your copyright notice in the Description page of Project Settings.


#include "Camera/CameraModule.h"

#include "Camera/CameraComponent.h"
#include "Camera/CameraModuleNetworkComponent.h"
#include "Camera/CameraModuleStatics.h"
#include "Camera/Actor/CCTVCameraActor.h"
#include "Camera/Actor/RoamCameraActor.h"
#include "Camera/Interface/CameraTrackableInterface.h"
#include "Camera/Manager/CameraManagerBase.h"
#include "Camera/Point/CameraPointBase.h"
#include "GameFramework/SpringArmComponent.h"
#include "Gameplay/WHGameMode.h"
#include "Gameplay/WHPlayerController.h"
#include "Common/CommonStatics.h"
#include "Event/EventModuleStatics.h"
#include "Event/Handle/Camera/EventHandle_CameraPointChanged.h"
#include "Event/Handle/Camera/EventHandle_ResetCameraView.h"
#include "Event/Handle/Camera/EventHandle_SetCameraView.h"
#include "Event/Handle/Camera/EventHandle_SwitchCameraPoint.h"
#include "Event/Handle/Camera/EventHandle_CameraTraceEnded.h"
#include "Input/InputModuleStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Main/MainModule.h"
#include "Math/MathStatics.h"
#include "SaveGame/SaveGameModuleStatics.h"
#include "SaveGame/Module/CameraSaveGame.h"
#include "Scene/SceneManager.h"
#include "Scene/SceneModuleStatics.h"

IMPLEMENTATION_MODULE(UCameraModule)

// Sets default values
UCameraModule::UCameraModule()
{
	ModuleName = FName("CameraModule");
	ModuleDisplayName = FText::FromString(TEXT("Camera Module"));

	bModuleRequired = true;

	ModuleSaveGame = UCameraSaveGame::StaticClass();

	ModuleNetworkComponent = UCameraModuleNetworkComponent::StaticClass();

	DefaultCamera = nullptr;
	bDefaultInstantSwitch = false;
	
	CameraClasses = TArray<TSubclassOf<ACameraActorBase>>();
	CameraClasses.Add(ARoamCameraActor::StaticClass());
	CameraClasses.Add(ACCTVCameraActor::StaticClass());
	
	Cameras = TArray<ACameraActorBase*>();
	CameraMap = TMap<FName, ACameraActorBase*>();
	CurrentCamera = nullptr;
	CurrentCameraPoint = nullptr;

	bCameraControlAble = true;

	bCameraMoveAble = true;
	bCameraMoveControlAble = true;
	bReverseCameraPanMove = false;
	bEnableCameraPanZMove = true;
	CameraMoveRange = FBox(EForceInit::ForceInitToZero);

#if WITH_EDITORONLY_DATA
	bDrawCameraRange = false;
#endif

	CameraMoveRate = 600.f;
	bSmoothCameraMove = true;
	CameraMoveSpeed = 5.f;
	CameraMoveAltitude = 0.f;

	bCameraOffsetAble = true;
	bSmoothCameraOffset = true;
	CameraOffsetSpeed = 5.f;
	InitCameraOffset = FVector::ZeroVector;

	bCameraRotateAble = true;
	bCameraRotateControlAble = true;
	bReverseCameraPitch = false;
	CameraTurnRate = 90.f;
	CameraLookUpRate = 90.f;
	bSmoothCameraRotate = true;
	CameraRotateSpeed = 5.f;
	MinCameraPitch = -90.f;
	MaxCameraPitch = 90.f;
	InitCameraPitch = -1.f;

	bCameraZoomAble = true;
	bCameraZoomControlAble = true;
	bNormalizeCameraZoom = false;
	CameraZoomRate = 300.f;
	bSmoothCameraZoom = true;
	CameraZoomSpeed = 5.f;
	MinCameraDistance = 0.f;
	MaxCameraDistance = -1.f;
	InitCameraDistance = -1.f;
	InitCameraFov = 0.f;
	CameraZoomAltitude = 0.f;

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

	DefaultCameraPoint = nullptr;

	PlayerController = nullptr;

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
}

UCameraModule::~UCameraModule()
{
	TERMINATION_MODULE(UCameraModule)
}

#if WITH_EDITOR
void UCameraModule::OnGenerate()
{
	Super::OnGenerate();

	// 获取场景Camera
	TArray<AActor*> ChildActors;
	GetModuleOwner()->GetAttachedActors(ChildActors);
	for(auto Iter : ChildActors)
	{
		if(auto Camera = Cast<ACameraActorBase>(Iter))
		{
			Cameras.AddUnique(Camera);
		}
	}

	// 移除废弃Camera
	TArray<ACameraActorBase*> RemoveList;
	for(auto Iter : Cameras)
	{
		if(!Iter || !CameraClasses.Contains(Iter->GetClass()))
		{
			RemoveList.AddUnique(Iter);
		}
	}
	for(auto Iter : RemoveList)
	{
		Cameras.Remove(Iter);
		if(Iter)
		{
			if(DefaultCamera == Iter)
			{
				DefaultCamera = nullptr;
			}
			Iter->Destroy();
		}
	}

	// 生成新的Camera
	for(auto Class : CameraClasses)
	{
		if(!Class) continue;

		bool bNeedSpawn = true;
		for(const auto Camera : Cameras)
		{
			if(Camera && Camera->IsA(Class))
			{
				bNeedSpawn = false;
				break;
			}
		}
		if(bNeedSpawn)
		{
			FActorSpawnParameters ActorSpawnParameters;
			ActorSpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if(ACameraActorBase* CameraPawn = GetWorld()->SpawnActor<ACameraActorBase>(Class, ActorSpawnParameters))
			{
				CameraPawn->SetActorLabel(CameraPawn->GetCameraName().ToString());
				CameraPawn->AttachToActor(GetModuleOwner(), FAttachmentTransformRules::KeepWorldTransform);
				Cameras.Add(CameraPawn);
			}
		}
	}

	Modify();
}

void UCameraModule::OnDestroy()
{
	Super::OnDestroy();

	TERMINATION_MODULE(UCameraModule)

	for(const auto Camera : Cameras)
	{
		if(Camera)
		{
			Camera->Destroy();
		}
	}
}
#endif

void UCameraModule::OnInitialize()
{
	Super::OnInitialize();

	IDebuggerInterface::Register();

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
	
	UEventModuleStatics::SubscribeEvent<UEventHandle_SetCameraView>(this, GET_FUNCTION_NAME_THISCLASS(OnSetCameraView));
	UEventModuleStatics::SubscribeEvent<UEventHandle_ResetCameraView>(this, GET_FUNCTION_NAME_THISCLASS(OnResetCameraView));
	UEventModuleStatics::SubscribeEvent<UEventHandle_SwitchCameraPoint>(this, GET_FUNCTION_NAME_THISCLASS(OnSwitchCameraPoint));
}

void UCameraModule::OnPreparatory(EPhase InPhase)
{
	Super::OnPreparatory(InPhase);

	if(PHASEC(InPhase, EPhase::Primary))
	{
		CurrentCameraLocation = GetRealCameraLocation();
		CurrentCameraRotation = GetRealCameraRotation();
		CurrentCameraDistance = GetRealCameraDistance();
	}
	if(PHASEC(InPhase, EPhase::Final))
	{
		if(DefaultCamera)
		{
			SwitchCamera(DefaultCamera, true, bDefaultInstantSwitch);
		}
		if(DefaultCameraPoint)
		{
			SwitchCameraPoint(DefaultCameraPoint);
		}
	}
}

void UCameraModule::OnRefresh(float DeltaSeconds, bool bInEditor)
{
	Super::OnRefresh(DeltaSeconds, bInEditor);

	if(bInEditor) return;

	DoTrackTarget();

	if(bCameraMoveAble && CurrentCamera)
	{
		if(CurrentCameraLocation != TargetCameraLocation)
		{
			if(CameraDoLocationDuration != 0.f)
			{
				CameraDoLocationTime = FMath::Clamp(CameraDoLocationTime + DeltaSeconds, 0.f, CameraDoLocationDuration);
				CurrentCamera->SetCameraLocation(FMath::Lerp(CameraDoLocationLocation, TargetCameraLocation, UMathStatics::EvaluateByEaseType(CameraDoLocationEaseType, CameraDoLocationTime, CameraDoLocationDuration)));
			}
			else
			{
				CurrentCamera->SetCameraLocation(!bSmoothCameraMove ? TargetCameraLocation : FMath::VInterpTo(CurrentCameraLocation, TargetCameraLocation, DeltaSeconds, CameraMoveSpeed));
			}
			CurrentCameraLocation = CurrentCamera->GetActorLocation();
		}
		else if(CameraDoLocationDuration != 0.f)
		{
			StopDoCameraLocation();
		}
	}

	if(bCameraOffsetAble && CurrentCamera)
	{
		if(CurrentCameraOffset != TargetCameraOffset)
		{
			if(CameraDoOffsetDuration != 0.f)
			{
				CameraDoOffsetTime = FMath::Clamp(CameraDoOffsetTime + DeltaSeconds, 0.f, CameraDoOffsetDuration);
				CurrentCamera->GetCameraBoom()->SocketOffset = FMath::Lerp(CameraDoOffsetOffset, TargetCameraOffset, UMathStatics::EvaluateByEaseType(CameraDoOffsetEaseType, CameraDoOffsetTime, CameraDoOffsetDuration));
			}
			else
			{
				CurrentCamera->GetCameraBoom()->SocketOffset = !bSmoothCameraOffset ? TargetCameraOffset : FMath::VInterpTo(CurrentCameraOffset, TargetCameraOffset, DeltaSeconds, CameraOffsetSpeed);
			}
			CurrentCameraOffset = CurrentCamera->GetCameraBoom()->SocketOffset;
		}
		else if(CameraDoOffsetDuration != 0.f)
		{
			StopDoCameraOffset();
		}
	}

	if(bCameraRotateAble && GetPlayerController())
	{
		if(!CurrentCameraRotation.Equals(TargetCameraRotation))
		{
			if(CameraDoRotationDuration != 0.f)
			{
				CameraDoRotationTime = FMath::Clamp(CameraDoRotationTime + DeltaSeconds, 0.f, CameraDoRotationDuration);
				GetPlayerController()->SetControlRotation(UMathStatics::LerpRotator(CameraDoRotationRotation, TargetCameraRotation, UMathStatics::EvaluateByEaseType(CameraDoRotationEaseType, CameraDoRotationTime, CameraDoRotationDuration), !CameraDoRotationRotation.Equals(TargetCameraRotation)));
			}
			else
			{
				GetPlayerController()->SetControlRotation(!bSmoothCameraRotate ? TargetCameraRotation : FMath::RInterpTo(CurrentCameraRotation, TargetCameraRotation, DeltaSeconds, CameraRotateSpeed));
			}
			CurrentCameraRotation = GetPlayerController()->GetControlRotation();
		}
		else if(CameraDoRotationDuration != 0.f)
		{
			StopDoCameraRotation();
		}
		if(CurrentCamera)
		{
			CurrentCamera->SetActorRotation(GetPlayerController()->GetControlRotation());
		}
	}

	if(bCameraZoomAble && CurrentCamera)
	{
		float TargetDistance = TargetCameraDistance;

		while(CameraMoveRange.IsValid && !CameraMoveRange.IsInsideOrOn(CurrentCameraLocation - CurrentCameraRotation.Vector() * TargetDistance) && TargetDistance > 0.f)
		{
			TargetDistance = FMath::Max(0.f, TargetDistance - 100.f);
		}
		
		if(CurrentCameraDistance != TargetDistance)
		{
			if(CameraDoDistanceDuration != 0.f)
			{
				CameraDoDistanceTime = FMath::Clamp(CameraDoDistanceTime + DeltaSeconds, 0.f, CameraDoDistanceDuration);
				CurrentCamera->GetCameraBoom()->TargetArmLength = FMath::Lerp(CameraDoDistanceDistance, TargetDistance, UMathStatics::EvaluateByEaseType(CameraDoDistanceEaseType, CameraDoDistanceTime, CameraDoDistanceDuration));
			}
			else
			{
				CurrentCamera->GetCameraBoom()->TargetArmLength = !bSmoothCameraZoom ? TargetDistance : FMath::FInterpTo(CurrentCameraDistance, TargetDistance, DeltaSeconds, bNormalizeCameraZoom && MaxCameraDistance != -1.f ? UKismetMathLibrary::NormalizeToRange(CurrentCamera->GetCameraBoom()->TargetArmLength, MinCameraDistance, MaxCameraDistance) * CameraZoomSpeed : CameraZoomSpeed);
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
				CameraDoFovTime = FMath::Clamp(CameraDoFovTime + DeltaSeconds, 0.f, CameraDoFovDuration);
				CurrentCamera->GetCamera()->SetFieldOfView(FMath::Lerp(CameraDoFovFov, TargetCameraFov, UMathStatics::EvaluateByEaseType(CameraDoFovEaseType, CameraDoFovTime, CameraDoFovDuration)));
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

void UCameraModule::OnPause()
{
	Super::OnPause();
}

void UCameraModule::OnUnPause()
{
	Super::OnUnPause();
}

void UCameraModule::OnTermination(EPhase InPhase)
{
	Super::OnTermination(InPhase);

	IDebuggerInterface::UnRegister();
}

void UCameraModule::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	auto& SaveData = InSaveData->CastRef<FCameraModuleSaveData>();

	bEnableCameraPanZMove = SaveData.bEnableCameraPanZMove;
	bReverseCameraPanMove = SaveData.bReverseCameraPanMove;
	CameraMoveRate = SaveData.CameraMoveRate;
	bSmoothCameraMove = SaveData.bSmoothCameraMove;
	CameraMoveSpeed = SaveData.CameraMoveSpeed;

	bReverseCameraPitch = SaveData.bReverseCameraPitch;
	CameraTurnRate = SaveData.CameraTurnRate;
	CameraLookUpRate = SaveData.CameraLookUpRate;
	bSmoothCameraRotate = SaveData.bSmoothCameraRotate;
	CameraRotateSpeed = SaveData.CameraRotateSpeed;

	CameraZoomRate = SaveData.CameraZoomRate;
	bSmoothCameraZoom = SaveData.bSmoothCameraZoom;
	CameraZoomSpeed = SaveData.CameraZoomSpeed;
}

void UCameraModule::UnloadData(EPhase InPhase)
{
}

FSaveData* UCameraModule::ToData()
{
	static FCameraModuleSaveData SaveData;
	SaveData = FCameraModuleSaveData();

	SaveData.bEnableCameraPanZMove = bEnableCameraPanZMove;
	SaveData.bReverseCameraPanMove = bReverseCameraPanMove;
	SaveData.CameraMoveRate = CameraMoveRate;
	SaveData.bSmoothCameraMove = bSmoothCameraMove;
	SaveData.CameraMoveSpeed = CameraMoveSpeed;

	SaveData.bReverseCameraPitch = bReverseCameraPitch;
	SaveData.CameraTurnRate = CameraTurnRate;
	SaveData.CameraLookUpRate = CameraLookUpRate;
	SaveData.bSmoothCameraRotate = bSmoothCameraRotate;
	SaveData.CameraRotateSpeed = CameraRotateSpeed;

	SaveData.CameraZoomRate = CameraZoomRate;
	SaveData.bSmoothCameraZoom = bSmoothCameraZoom;
	SaveData.CameraZoomSpeed = CameraZoomSpeed;
	
	return &SaveData;
}

FString UCameraModule::GetModuleDebugMessage()
{
	return FString::Printf(TEXT("CurrentCamera: %s"), CurrentCamera ? *CurrentCamera->GetCameraName().ToString() : TEXT("None"));
}

void UCameraModule::OnDrawDebug(UCanvas* InCanvas, APlayerController* InPC)
{
	if(bDrawCameraRange)
	{
		UKismetSystemLibrary::DrawDebugBox(this, CameraMoveRange.GetCenter(), CameraMoveRange.GetExtent(), FLinearColor::Red);
	}
}

ACameraActorBase* UCameraModule::GetCurrentCamera(TSubclassOf<ACameraActorBase> InClass) const
{
	return GetDeterminesOutputObject(CurrentCamera, InClass);
}

ACameraManagerBase* UCameraModule::GetCurrentCameraManager()
{
	if(GetPlayerController())
	{
		return Cast<ACameraManagerBase>(GetPlayerController()->PlayerCameraManager);
	}
	return nullptr;
}

ACameraActorBase* UCameraModule::GetCameraByClass(TSubclassOf<ACameraActorBase> InClass)
{
	if(!InClass) return nullptr;
	
	const FName CameraName = InClass.GetDefaultObject()->GetCameraName();
	return GetCameraByName(CameraName);
}

ACameraActorBase* UCameraModule::GetCameraByName(const FName InName) const
{
	if(CameraMap.Contains(InName))
	{
		return CameraMap[InName];
	}
	return nullptr;
}

void UCameraModule::SwitchCamera(ACameraActorBase* InCamera, bool bReset, bool bInstant)
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
		if(GetPlayerController())
		{
			GetPlayerController()->SetViewTarget(InCamera);
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
		if(GetPlayerController())
		{
			GetPlayerController()->SetViewTarget(nullptr);
		}
		CurrentCamera->OnUnSwitch();
		CurrentCamera = nullptr;
	}
}

void UCameraModule::SwitchCameraByClass(TSubclassOf<ACameraActorBase> InClass, bool bReset, bool bInstant)
{
	const FName CameraName = InClass ? InClass.GetDefaultObject()->GetCameraName() : NAME_None;
	SwitchCameraByName(CameraName, bReset, bInstant);
}

void UCameraModule::SwitchCameraByName(const FName InName, bool bReset, bool bInstant)
{
	SwitchCamera(GetCameraByName(InName), bReset, bInstant);
}

void UCameraModule::SwitchCameraPoint(ACameraPointBase* InCameraPoint, bool bSetAsDefault, bool bInstant)
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

void UCameraModule::DoTrackTarget(bool bInstant)
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

void UCameraModule::DoTrackTargetLocation(bool bInstant)
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

void UCameraModule::DoTrackTargetRotation(bool bInstant)
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

void UCameraModule::DoTrackTargetDistance(bool bInstant)
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

void UCameraModule::StartTrackTarget(AActor* InTargetActor, ECameraTrackMode InTrackMode, ECameraViewMode InViewMode, ECameraViewSpace InViewSpace, FVector InLocation, FVector InOffset, float InYaw, float InPitch, float InDistance, bool bAllowControl, EEaseType InViewEaseType, float InViewDuration, bool bInstant)
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

void UCameraModule::EndTrackTarget(AActor* InTargetActor)
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

void UCameraModule::SetCameraLocation(FVector InLocation, bool bInstant)
{
	if(!CurrentCamera) return;

	TargetCameraLocation = CameraMoveRange.IsValid && !IsTrackingTarget() ? ClampVector(InLocation, CameraMoveRange.Min, CameraMoveRange.Max) : InLocation;
	if(bInstant)
	{
		CurrentCameraLocation = TargetCameraLocation;
		CurrentCamera->SetCameraLocation(TargetCameraLocation);
	}
	StopDoCameraLocation();
}

void UCameraModule::DoCameraLocation(FVector InLocation, float InDuration, EEaseType InEaseType, bool bForce)
{
	if(!CurrentCamera || ((CameraDoLocationLocation != EMPTY_Vector || CurrentCameraLocation == InLocation) && !bForce)) return;

	TargetCameraLocation = CameraMoveRange.IsValid && !IsTrackingTarget() ? ClampVector(InLocation, CameraMoveRange.Min, CameraMoveRange.Max) : InLocation;
	if(InDuration > 0.f)
	{
		CameraDoLocationTime = 0.f;
		CameraDoLocationDuration = InDuration;
		CameraDoLocationLocation = CurrentCameraLocation;
		CameraDoLocationEaseType = InEaseType;
	}
	else if(CurrentCamera && CurrentCamera->IsA<ACameraActorBase>())
	{
		CurrentCameraLocation = TargetCameraLocation;
		CurrentCamera->SetCameraLocation(TargetCameraLocation);
	}
}

void UCameraModule::StopDoCameraLocation()
{
	CameraDoLocationTime = 0.f;
	CameraDoLocationDuration = 0.f;
	CameraDoLocationLocation = EMPTY_Vector;
}

void UCameraModule::SetCameraOffset(FVector InOffset, bool bInstant)
{
	if(!CurrentCamera) return;

	InOffset = InOffset != FVector(-1.f) ? InOffset : (IsTrackingTarget() ? TrackCameraViewData.CameraViewParams.CameraViewOffset : InitCameraOffset);

	TargetCameraOffset = CameraMoveRange.IsValid && !IsTrackingTarget() ? ClampVector(InOffset, CameraMoveRange.Min, CameraMoveRange.Max) : InOffset;
	if(bInstant)
	{
		CurrentCameraOffset = TargetCameraOffset;
		CurrentCamera->GetCameraBoom()->SocketOffset = TargetCameraOffset;
	}
	StopDoCameraOffset();
}

void UCameraModule::DoCameraOffset(FVector InOffset, float InDuration, EEaseType InEaseType, bool bForce)
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
	else if(CurrentCamera && CurrentCamera->IsA<ACameraActorBase>())
	{
		CurrentCameraOffset = TargetCameraOffset;
		CurrentCamera->GetCameraBoom()->SocketOffset = TargetCameraOffset;
	}
}

void UCameraModule::StopDoCameraOffset()
{
	CameraDoOffsetTime = 0.f;
	CameraDoOffsetDuration = 0.f;
	CameraDoOffsetOffset = EMPTY_Vector;
}

void UCameraModule::SetCameraRotation(float InYaw, float InPitch, bool bInstant)
{
	if(!GetPlayerController()) return;
	
	const FRotator TargetRotator = FRotator(FMath::Clamp(InPitch == -1.f ? (InitCameraPitch == -1.f ? CurrentCameraRotation.Pitch : InitCameraPitch) : InPitch, GetMinCameraPitch(), GetMaxCameraPitch()), InYaw == -1.f ? CurrentCameraRotation.Yaw : InYaw, CurrentCameraRotation.Roll);

	if(TargetRotator.Equals(TargetCameraRotation)) return;

	TargetCameraRotation = TargetRotator;

	if(bInstant)
	{
		CurrentCameraRotation = TargetCameraRotation;
		GetPlayerController()->SetControlRotation(TargetCameraRotation);
		if(CurrentCamera)
		{
			CurrentCamera->SetActorRotation(TargetCameraRotation);
		}
	}
	StopDoCameraRotation();
}

void UCameraModule::DoCameraRotation(float InYaw, float InPitch, float InDuration, EEaseType InEaseType, bool bForce)
{
	if(!GetPlayerController() || (CameraDoRotationRotation != EMPTY_Rotator || (CurrentCameraRotation.Yaw == InYaw && CurrentCameraRotation.Pitch == InPitch))  && !bForce) return;

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
		GetPlayerController()->SetControlRotation(TargetCameraRotation);
		if(CurrentCamera)
		{
			CurrentCamera->SetActorRotation(TargetCameraRotation);
		}
	}
}

void UCameraModule::StopDoCameraRotation()
{
	CameraDoRotationTime = 0.f;
	CameraDoRotationDuration = 0.f;
	CameraDoRotationRotation = EMPTY_Rotator;
}

void UCameraModule::SetCameraDistance(float InDistance, bool bInstant)
{
	if(!CurrentCamera) return;

	TargetCameraDistance = InDistance != -1.f ? FMath::Clamp(InDistance, MinCameraDistance, MaxCameraDistance == -1.f ? FLT_MAX : MaxCameraDistance) : InitCameraDistance;
	while(CameraMoveRange.IsValid && !CameraMoveRange.IsInsideOrOn(CurrentCameraLocation - CurrentCameraRotation.Vector() * TargetCameraDistance) && TargetCameraDistance > 0.f)
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

void UCameraModule::DoCameraDistance(float InDistance, float InDuration, EEaseType InEaseType, bool bForce)
{
	if(!CurrentCamera || (CameraDoDistanceDistance != EMPTY_Flt || CurrentCameraDistance == InDistance)  && !bForce) return;

	TargetCameraDistance = InDistance != -1.f ? FMath::Clamp(InDistance, MinCameraDistance, MaxCameraDistance == -1.f ? FLT_MAX : MaxCameraDistance) : InitCameraDistance;
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

void UCameraModule::StopDoCameraDistance()
{
	CameraDoDistanceTime = 0.f;
	CameraDoDistanceDuration = 0.f;
	CameraDoDistanceDistance = EMPTY_Flt;
}

void UCameraModule::SetCameraRotationAndDistance(float InYaw, float InPitch, float InDistance, bool bInstant)
{
	SetCameraRotation(InYaw, InPitch, bInstant);
	SetCameraDistance(InDistance, bInstant);
}

void UCameraModule::DoCameraRotationAndDistance(float InYaw, float InPitch, float InDistance, float InDuration, EEaseType InEaseType, bool bForce)
{
	DoCameraRotation(InYaw, InPitch, InDuration, InEaseType, bForce);
	DoCameraDistance(InDistance, InDuration, InEaseType, bForce);
}

void UCameraModule::SetCameraTransform(FVector InLocation, float InYaw, float InPitch, float InDistance, bool bInstant)
{
	SetCameraLocation(InLocation, bInstant);
	SetCameraRotation(InYaw, InPitch, bInstant);
	SetCameraDistance(InDistance, bInstant);
}

void UCameraModule::DoCameraTransform(FVector InLocation, float InYaw, float InPitch, float InDistance, float InDuration, EEaseType InEaseType, bool bForce)
{
	DoCameraLocation(InLocation, InDuration, InEaseType, bForce);
	DoCameraRotation(InYaw, InPitch, InDuration, InEaseType, bForce);
	DoCameraDistance(InDistance, InDuration, InEaseType, bForce);
}

void UCameraModule::StopDoCameraTransform()
{
	StopDoCameraLocation();
	StopDoCameraRotation();
	StopDoCameraDistance();
}

void UCameraModule::SetCameraFov(float InFov, bool bInstant)
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

void UCameraModule::DoCameraFov(float InFov, float InDuration, EEaseType InEaseType, bool bForce)
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

void UCameraModule::StopDoCameraFov()
{
	CameraDoFovTime = 0.f;
	CameraDoFovDuration = 0.f;
	CameraDoFovFov = EMPTY_Flt;
}

void UCameraModule::AddCameraMovementInput(FVector InDirection, float InValue)
{
	if(!bCameraControlAble || !bCameraMoveControlAble || (IsTrackingTarget() && !ENUMWITH(TrackControlMode, ECameraControlMode::LocationOnly))) return;

	SetCameraLocation(TargetCameraLocation + InDirection * InValue * CameraMoveRate * (1.f + (CameraMoveAltitude != 0.f ? (UCommonStatics::GetPossessedPawn() ? 0.f : FMath::Abs(USceneModuleStatics::GetAltitude(false, true)) / CameraMoveAltitude) : 0.f)) * GetWorld()->GetDeltaSeconds(), false);
}

void UCameraModule::AddCameraRotationInput(float InYaw, float InPitch)
{
	if(!bCameraControlAble || !bCameraRotateControlAble || (IsTrackingTarget() && !ENUMWITH(TrackControlMode, ECameraControlMode::RotationOnly))) return;

	SetCameraRotation(TargetCameraRotation.Yaw + InYaw * CameraTurnRate * GetWorld()->GetDeltaSeconds(), TargetCameraRotation.Pitch + InPitch * CameraLookUpRate * GetWorld()->GetDeltaSeconds(), false);
}

void UCameraModule::AddCameraDistanceInput(float InValue, bool bMoveIfZero)
{
	if(!bCameraControlAble || !bCameraZoomControlAble || (IsTrackingTarget() && !ENUMWITH(TrackControlMode, ECameraControlMode::DistanceOnly))) return;

	SetCameraDistance(TargetCameraDistance + InValue * CameraZoomRate * (2.f + (CameraZoomAltitude != 0.f ? (UCommonStatics::GetPossessedPawn() ? 0.f : FMath::Abs(FMath::Max(USceneModuleStatics::GetAltitude(false, true), CurrentCameraDistance)) / CameraZoomAltitude) : 0.f)) * GetWorld()->GetDeltaSeconds(), false);

	if(bMoveIfZero && InValue < 0.f && TargetCameraDistance == 0.f)
	{
		AddCameraMovementInput(CurrentCameraRotation.Vector(), -InValue);
	}
}

void UCameraModule::SetCameraView(const FCameraViewData& InCameraViewData, bool bCacheData, bool bInstant)
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

void UCameraModule::SetCameraViewParams(const FCameraViewParams& InCameraViewParams, bool bInstant)
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

void UCameraModule::ResetCameraView(ECameraResetMode InCameraResetMode, bool bInstant)
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

void UCameraModule::OnSetCameraView(UObject* InSender, UEventHandle_SetCameraView* InEventHandle)
{
	SetCameraView(InEventHandle->CameraViewData, InEventHandle->bCacheData);
}

void UCameraModule::OnResetCameraView(UObject* InSender, UEventHandle_ResetCameraView* InEventHandle)
{
	ResetCameraView(InEventHandle->CameraResetMode);
}

void UCameraModule::OnSwitchCameraPoint(UObject* InSender, UEventHandle_SwitchCameraPoint* InEventHandle)
{
	SwitchCameraPoint(InEventHandle->CameraPoint.LoadSynchronous());
}

bool UCameraModule::IsControllingMove()
{
	return UInputModuleStatics::GetKeyShortcut(GameplayTags::Input_CameraPanMove).IsPressing(GetPlayerController()) || UInputModuleStatics::GetTouchPressedCount() == 3;
}

bool UCameraModule::IsControllingRotate()
{
	return UInputModuleStatics::GetKeyShortcut(GameplayTags::Input_CameraRotate).IsPressing(GetPlayerController()) || UInputModuleStatics::GetTouchPressedCount() == 1;
}

bool UCameraModule::IsControllingZoom()
{
	return UInputModuleStatics::GetKeyShortcut(GameplayTags::Input_CameraZoom).IsPressing(GetPlayerController()) || UInputModuleStatics::GetTouchPressedCount() == 2;
}

bool UCameraModule::IsTrackingTarget()
{
	return TrackCameraViewData.CameraViewParams.CameraViewTarget != nullptr;
}

float UCameraModule::GetMinCameraPitch() const
{
	return GetTrackingTarget() && GetTrackingTarget()->Implements<UCameraTrackableInterface>() && ICameraTrackableInterface::Execute_GetCameraMinPitch(GetTrackingTarget()) != -1.f ? ICameraTrackableInterface::Execute_GetCameraMinPitch(GetTrackingTarget()) : MinCameraPitch;
}

float UCameraModule::GetMaxCameraPitch() const
{
	return GetTrackingTarget() && GetTrackingTarget()->Implements<UCameraTrackableInterface>() && ICameraTrackableInterface::Execute_GetCameraMaxPitch(GetTrackingTarget()) != -1.f ? ICameraTrackableInterface::Execute_GetCameraMaxPitch(GetTrackingTarget()) : MaxCameraPitch;
}

FVector UCameraModule::GetRealCameraLocation()
{
	if(GetCurrentCameraManager())
	{
		return GetCurrentCameraManager()->GetCameraLocation();
	}
	return FVector::ZeroVector;
}

FVector UCameraModule::GetCurrentCameraLocation(bool bRefresh) const
{
	if(bRefresh)
	{
		return CurrentCamera ? CurrentCamera->GetActorLocation() : FVector::ZeroVector;
	}
	return CurrentCameraLocation;
}

FVector UCameraModule::GetRealCameraOffset() const
{
	if(CurrentCamera && CurrentCamera->GetCameraBoom())
	{
		return CurrentCamera->GetCameraBoom()->SocketOffset;
	}
	return FVector::ZeroVector;
}

FVector UCameraModule::GetCurrentCameraOffset(bool bRefresh) const
{
	if(bRefresh)
	{
		return CurrentCamera ? CurrentCamera->GetCameraBoom()->SocketOffset : FVector::ZeroVector;
	}
	return CurrentCameraOffset;
}

FRotator UCameraModule::GetRealCameraRotation()
{
	if(GetCurrentCameraManager())
	{
		return GetCurrentCameraManager()->GetCameraRotation();
	}
	return FRotator::ZeroRotator;
}

FRotator UCameraModule::GetCurrentCameraRotation(bool bRefresh)
{
	if(bRefresh)
	{
		return GetPlayerController() ? GetPlayerController()->GetControlRotation() : FRotator::ZeroRotator;
	}
	return CurrentCameraRotation;
}

float UCameraModule::GetRealCameraDistance() const
{
	if(CurrentCamera && CurrentCamera->GetCameraBoom())
	{
		return CurrentCamera->GetCameraBoom()->TargetArmLength;
	}
	return 0.f;
}

float UCameraModule::GetCurrentCameraDistance(bool bRefresh) const
{
	if(bRefresh)
	{
		return CurrentCamera ? CurrentCamera->GetCameraBoom()->TargetArmLength : 0;
	}
	return CurrentCameraDistance;
}

float UCameraModule::GetRealCameraFov() const
{
	if(CurrentCamera && CurrentCamera->GetCameraBoom())
	{
		return CurrentCamera->GetCamera()->FieldOfView;
	}
	return 0.f;
}

float UCameraModule::GetCurrentCameraFov(bool bRefresh) const
{
	if(bRefresh)
	{
		return CurrentCamera ? CurrentCamera->GetCamera()->FieldOfView : 0;
	}
	return CurrentCameraFov;
}

AWHPlayerController* UCameraModule::GetPlayerController()
{
	if(!PlayerController)
	{
		PlayerController = UCommonStatics::GetPlayerController<AWHPlayerController>();
	}
	return PlayerController;
}

void UCameraModule::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

}

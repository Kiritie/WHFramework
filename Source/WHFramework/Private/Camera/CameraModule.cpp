// Fill out your copyright notice in the Description page of Project Settings.


#include "Camera/CameraModule.h"

#include "Camera/CameraModuleNetworkComponent.h"
#include "Camera/CameraModuleStatics.h"
#include "Camera/Actor/Roam/RoamCameraActor.h"
#include "Camera/Manager/CameraManagerBase.h"
#include "Camera/Point/CameraPointBase.h"
#include "GameFramework/SpringArmComponent.h"
#include "Gameplay/WHGameMode.h"
#include "Gameplay/WHPlayerController.h"
#include "Common/CommonStatics.h"
#include "Event/EventModuleStatics.h"
#include "Event/Handle/Camera/EventHandle_ResetCameraView.h"
#include "Event/Handle/Camera/EventHandle_SetCameraView.h"
#include "Event/Handle/Camera/EventHandle_SwitchCameraPoint.h"
#include "Input/InputModuleStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Main/MainModule.h"
#include "Math/MathStatics.h"
#include "SaveGame/SaveGameModuleStatics.h"
#include "SaveGame/Module/CameraSaveGame.h"

IMPLEMENTATION_MODULE(UCameraModule)

// Sets default values
UCameraModule::UCameraModule()
{
	ModuleName = FName("CameraModule");
	ModuleDisplayName = FText::FromString(TEXT("Camera Module"));

	ModuleSaveGame = UCameraSaveGame::StaticClass();

	ModuleNetworkComponent = UCameraModuleNetworkComponent::StaticClass();

	DefaultCamera = nullptr;
	DefaultInstantSwitch = false;
	CameraClasses = TArray<TSubclassOf<ACameraActorBase>>();
	CameraClasses.Add(ARoamCameraActor::StaticClass());
	Cameras = TArray<ACameraActorBase*>();
	CameraMap = TMap<FName, ACameraActorBase*>();
	CurrentCamera = nullptr;

	bCameraControlAble = true;

	bCameraMoveAble = true;
	bCameraMoveControlAble = true;
	bReverseCameraPanMove = false;
	bClampCameraMove = false;
	CameraMoveRange = FBox(EForceInit::ForceInitToZero);
	CameraMoveRate = 600.f;
	bSmoothCameraMove = true;
	CameraMoveSpeed = 5.f;

	bCameraRotateAble = true;
	bCameraRotateControlAble = true;
	bReverseCameraPitch = false;
	CameraTurnRate = 90.f;
	CameraLookUpRate = 90.f;
	bSmoothCameraRotate = true;
	CameraRotateSpeed = 5.f;
	MinCameraPitch = -89.f;
	MaxCameraPitch = 89.f;
	InitCameraPitch = -1.f;

	bCameraZoomAble = true;
	bCameraZoomControlAble = true;
	bNormalizeCameraZoom = false;
	CameraZoomRate = 300.f;
	bSmoothCameraZoom = true;
	CameraZoomSpeed = 5.f;
	MinCameraDistance = 0.f;
	MaxCameraDistance = -1.f;
	InitCameraDistance = 0.f;
	CachedCameraParams = FCameraViewParams();

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

	bTrackAllowControl = false;
	TrackTargetActor = nullptr;
	TrackLocationOffset = FVector::ZeroVector;
	TrackYawOffset = 0.f;
	TrackPitchOffset = 0.f;
	TrackDistance = 0.f;
	TrackTargetMode = ECameraTrackMode::LocationOnly;

	bIsControllingMove = false;
	bIsControllingRotate = false;
	bIsControllingZoom = false;

	PlayerController = nullptr;

	CurrentCameraLocation = FVector::ZeroVector;
	CurrentCameraRotation = FRotator::ZeroRotator;
	CurrentCameraDistance = 0.f;

	TargetCameraLocation = FVector::ZeroVector;
	TargetCameraRotation = FRotator::ZeroRotator;
	TargetCameraDistance = 0.f;
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

	for(auto Iter : Cameras)
	{
		if(Iter && !CameraMap.Contains(Iter->GetCameraName()))
		{
			CameraMap.Add(Iter->GetCameraName(), Iter);
		}
	}
	
	UEventModuleStatics::SubscribeEvent<UEventHandle_SetCameraView>(this, FName("OnSetCameraView"));
	UEventModuleStatics::SubscribeEvent<UEventHandle_ResetCameraView>(this, FName("OnResetCameraView"));
	UEventModuleStatics::SubscribeEvent<UEventHandle_SwitchCameraPoint>(this, FName("OnSwitchCameraPoint"));
}

void UCameraModule::OnPreparatory(EPhase InPhase)
{
	Super::OnPreparatory(InPhase);

	if(PHASEC(InPhase, EPhase::Final))
	{
		if(DefaultCamera)
		{
			SwitchCamera(DefaultCamera, DefaultInstantSwitch);
		}
		if(DefaultCameraPoint)
		{
			SwitchCameraPoint(DefaultCameraPoint);
		}
	}
}

void UCameraModule::OnRefresh(float DeltaSeconds)
{
	Super::OnRefresh(DeltaSeconds);

	if(!bCameraControlAble) return;

	DoTrackTarget(false);

	if(bCameraMoveAble && CurrentCamera)
	{
		CurrentCameraLocation = CurrentCamera->GetActorLocation();
		if(CurrentCameraLocation != TargetCameraLocation)
		{
			if(CameraDoMoveDuration != 0.f)
			{
				CameraDoMoveTime = FMath::Clamp(CameraDoMoveTime + DeltaSeconds, 0.f, CameraDoMoveDuration);
				CurrentCamera->SetCameraLocation(FMath::Lerp(CameraDoMoveLocation, TargetCameraLocation, UMathStatics::EvaluateByEaseType(CameraDoMoveEaseType, CameraDoMoveTime, CameraDoMoveDuration)));
			}
			else
			{
				CurrentCamera->SetCameraLocation(!bSmoothCameraMove ? TargetCameraLocation : FMath::VInterpTo(CurrentCamera->GetActorLocation(), TargetCameraLocation, DeltaSeconds, CameraMoveSpeed));
			}
		}
		else if(CameraDoMoveDuration != 0.f)
		{
			StopDoCameraLocation();
		}
	}

	if(bCameraRotateAble && GetPlayerController())
	{
		CurrentCameraRotation = GetPlayerController()->GetControlRotation();
		if(CurrentCameraRotation != TargetCameraRotation)
		{
			if(CameraDoRotateDuration != 0.f)
			{
				CameraDoRotateTime = FMath::Clamp(CameraDoRotateTime + DeltaSeconds, 0.f, CameraDoRotateDuration);
				GetPlayerController()->SetControlRotation(UMathStatics::LerpRotator(CameraDoRotateRotation, TargetCameraRotation, UMathStatics::EvaluateByEaseType(CameraDoRotateEaseType, CameraDoRotateTime, CameraDoRotateDuration), !CameraDoRotateRotation.Equals(TargetCameraRotation)));
			}
			else
			{
				GetPlayerController()->SetControlRotation(!bSmoothCameraRotate ? TargetCameraRotation : FMath::RInterpTo(GetPlayerController()->GetControlRotation(), TargetCameraRotation, DeltaSeconds, CameraRotateSpeed));
			}
		}
		else if(CameraDoRotateDuration != 0.f)
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
		CurrentCameraDistance = CurrentCamera->GetCameraBoom()->TargetArmLength;
		if(CurrentCameraDistance != TargetCameraDistance)
		{
			if(CameraDoZoomDuration != 0.f)
			{
				CameraDoZoomTime = FMath::Clamp(CameraDoZoomTime + DeltaSeconds, 0.f, CameraDoZoomDuration);
				CurrentCamera->GetCameraBoom()->TargetArmLength = FMath::Lerp(CameraDoZoomDistance, TargetCameraDistance, UMathStatics::EvaluateByEaseType(CameraDoZoomEaseType, CameraDoZoomTime, CameraDoZoomDuration));
			}
			else
			{
				CurrentCamera->GetCameraBoom()->TargetArmLength = !bSmoothCameraZoom ? TargetCameraDistance : FMath::FInterpTo(CurrentCamera->GetCameraBoom()->TargetArmLength, TargetCameraDistance, DeltaSeconds, bNormalizeCameraZoom && MaxCameraDistance != -1.f ? UKismetMathLibrary::NormalizeToRange(CurrentCamera->GetCameraBoom()->TargetArmLength, MinCameraDistance, MaxCameraDistance) * CameraZoomSpeed : CameraZoomSpeed);
			}
		}
		else if(CameraDoZoomDuration != 0.f)
		{
			StopDoCameraDistance();
		}
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
}

void UCameraModule::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	auto& SaveData = InSaveData->CastRef<FCameraModuleSaveData>();

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

void UCameraModule::SwitchCamera(ACameraActorBase* InCamera, bool bInstant)
{
	if(!CurrentCamera || CurrentCamera != InCamera)
	{
		if(InCamera)
		{
			CurrentCamera = InCamera;
			if(GetPlayerController())
			{
				GetPlayerController()->SetViewTarget(InCamera);
			}
			InitSocketOffset = CurrentCamera->GetCameraBoom()->SocketOffset;
			SetCameraLocation(InCamera->GetActorLocation(), bInstant);
			SetCameraRotation(InCamera->GetActorRotation().Yaw, InCamera->GetActorRotation().Pitch, bInstant);
			SetCameraDistance(InitCameraDistance, bInstant);
		}
		else if(CurrentCamera)
		{
			if(GetPlayerController())
			{
				GetPlayerController()->SetViewTarget(nullptr);
			}
			CurrentCamera = nullptr;
		}
	}
}

void UCameraModule::SwitchCameraByClass(TSubclassOf<ACameraActorBase> InClass, bool bInstant)
{
	const FName CameraName = InClass ? InClass.GetDefaultObject()->GetCameraName() : NAME_None;
	SwitchCameraByName(CameraName, bInstant);
}

void UCameraModule::SwitchCameraByName(const FName InName, bool bInstant)
{
	SwitchCamera(GetCameraByName(InName), bInstant);
}

void UCameraModule::DoTrackTarget(bool bInstant)
{
	if(!TrackTargetActor) return;
	
	switch(TrackTargetMode)
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
			TrackTargetMode = ECameraTrackMode::LocationOnly;
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
			TrackTargetMode = ECameraTrackMode::LocationAndRotation;
			break;
		}
		case ECameraTrackMode::LocationAndRotationOnceAndDistanceOnce:
		{
			DoTrackTargetLocation(bInstant);
			DoTrackTargetRotation(bInstant);
			DoTrackTargetDistance(bInstant);
			TrackTargetMode = ECameraTrackMode::LocationOnly;
			break;
		}
	}
}

void UCameraModule::DoTrackTargetLocation(bool bInstant)
{
	if(!TrackTargetActor) return;

	if(!bTrackAllowControl || !IsControllingMove())
	{
		SetCameraLocation(TrackTargetActor->GetActorLocation() + TrackTargetActor->GetActorRotation().RotateVector(TrackLocationOffset), bInstant);
	}
}

void UCameraModule::DoTrackTargetRotation(bool bInstant)
{
	if(!TrackTargetActor) return;

	if(!bTrackAllowControl || !IsControllingRotate())
	{
		SetCameraRotation(TrackTargetActor->GetActorRotation().Yaw + TrackYawOffset, TrackTargetActor->GetActorRotation().Pitch + TrackPitchOffset, bInstant);
	}
}

void UCameraModule::DoTrackTargetDistance(bool bInstant)
{
	if(!TrackTargetActor) return;

	if(!bTrackAllowControl || !IsControllingZoom())
	{
		SetCameraDistance(TrackDistance, bInstant);
	}
}

void UCameraModule::StartTrackTarget(AActor* InTargetActor, ECameraTrackMode InTrackTargetMode, ECameraViewSpace InTrackTargetSpace, FVector InLocationOffset, FVector InSocketOffset, float InYawOffset, float InPitchOffset, float InDistance, bool bAllowControl, bool bInstant)
{
	if(!InTargetActor || !CurrentCamera) return;

	if(TrackTargetActor != InTargetActor)
	{
		TrackTargetActor = InTargetActor;
		TrackTargetMode = InTrackTargetMode;
		TrackLocationOffset = InLocationOffset == FVector(-1.f) ? TargetCameraLocation - InTargetActor->GetActorLocation() : InTrackTargetSpace == ECameraViewSpace::Local ? InLocationOffset : InLocationOffset - InTargetActor->GetActorLocation();
		CurrentCamera->GetCameraBoom()->SocketOffset = InSocketOffset == FVector(-1.f) ? InitSocketOffset : InSocketOffset;
		TrackYawOffset = InYawOffset == -1.f ? TargetCameraRotation.Yaw - InTargetActor->GetActorRotation().Yaw : InTrackTargetSpace == ECameraViewSpace::Local ? InYawOffset : InYawOffset - InTargetActor->GetActorRotation().Yaw;
		TrackPitchOffset = InPitchOffset == -1.f ? TargetCameraRotation.Pitch - InTargetActor->GetActorRotation().Pitch : InTrackTargetSpace == ECameraViewSpace::Local ? InPitchOffset : InPitchOffset - InTargetActor->GetActorRotation().Pitch;
		TrackDistance = InDistance == -1.f ? TargetCameraDistance : InDistance;
		bTrackAllowControl = bAllowControl;
		DoTrackTarget(bInstant);
	}
}

void UCameraModule::EndTrackTarget(AActor* InTargetActor)
{
	if(!CurrentCamera) return;

	if(!InTargetActor || InTargetActor == TrackTargetActor)
	{
		TrackTargetActor = nullptr;
	}
}

void UCameraModule::SetCameraLocation(FVector InLocation, bool bInstant)
{
	if(!CurrentCamera) return;

	TargetCameraLocation = bClampCameraMove ? ClampVector(InLocation, CameraMoveRange.Min, CameraMoveRange.Max) : InLocation;
	if(bInstant)
	{
		CurrentCameraLocation = TargetCameraLocation;
		CurrentCamera->SetCameraLocation(TargetCameraLocation);
	}
	StopDoCameraLocation();
}

void UCameraModule::DoCameraLocation(FVector InLocation, float InDuration, EEaseType InEaseType)
{
	if(!CurrentCamera) return;

	TargetCameraLocation = bClampCameraMove ? ClampVector(InLocation, CameraMoveRange.Min, CameraMoveRange.Max) : InLocation;
	if(InDuration > 0.f)
	{
		CameraDoMoveTime = 0.f;
		CameraDoMoveDuration = InDuration;
		CameraDoMoveLocation = CurrentCameraLocation;
		CameraDoMoveEaseType = InEaseType;
	}
	else if(CurrentCamera && CurrentCamera->IsA<ACameraActorBase>())
	{
		CurrentCameraLocation = TargetCameraLocation;
		CurrentCamera->SetCameraLocation(TargetCameraLocation);
	}
}

void UCameraModule::StopDoCameraLocation()
{
	CameraDoMoveTime = 0.f;
	CameraDoMoveDuration = 0.f;
	CameraDoMoveLocation = FVector::ZeroVector;
}

void UCameraModule::SetCameraRotation(float InYaw, float InPitch, bool bInstant)
{
	if(!GetPlayerController()) return;
	
	TargetCameraRotation = FRotator(FMath::Clamp(InPitch == -1.f ? (InitCameraPitch == -1.f ? CurrentCameraRotation.Pitch : InitCameraPitch) : InPitch, MinCameraPitch, MaxCameraPitch), InYaw == -1.f ? CurrentCameraRotation.Yaw : InYaw, CurrentCameraRotation.Roll);
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

void UCameraModule::DoCameraRotation(float InYaw, float InPitch, float InDuration, EEaseType InEaseType)
{
	if(!GetPlayerController()) return;
	
	TargetCameraRotation = FRotator(FMath::Clamp(InPitch == -1.f ? (InitCameraPitch == -1.f ? CurrentCameraRotation.Pitch : InitCameraPitch) : InPitch, MinCameraPitch, MaxCameraPitch), InYaw == -1.f ? CurrentCameraRotation.Yaw : InYaw, CurrentCameraRotation.Roll);
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
		if(CurrentCamera)
		{
			CurrentCamera->SetActorRotation(TargetCameraRotation);
		}
	}
}

void UCameraModule::StopDoCameraRotation()
{
	CameraDoRotateTime = 0.f;
	CameraDoRotateDuration = 0.f;
	CameraDoRotateRotation = FRotator::ZeroRotator;
}

void UCameraModule::SetCameraDistance(float InDistance, bool bInstant)
{
	if(!CurrentCamera) return;

	TargetCameraDistance = InDistance != -1.f ? FMath::Clamp(InDistance, MinCameraDistance, MaxCameraDistance == -1.f ? FLT_MAX : MaxCameraDistance) : InitCameraDistance;
	if(bInstant)
	{
		CurrentCameraDistance = TargetCameraDistance;
		CurrentCamera->GetCameraBoom()->TargetArmLength = TargetCameraDistance;
	}
	StopDoCameraDistance();
}

void UCameraModule::DoCameraDistance(float InDistance, float InDuration, EEaseType InEaseType)
{
	if(!CurrentCamera) return;

	TargetCameraDistance = InDistance != -1.f ? FMath::Clamp(InDistance, MinCameraDistance, MaxCameraDistance == -1.f ? FLT_MAX : MaxCameraDistance) : InitCameraDistance;
	if(InDuration > 0.f)
	{
		CameraDoZoomTime = 0.f;
		CameraDoZoomDuration = InDuration;
		CameraDoZoomDistance = CurrentCameraDistance;
		CameraDoZoomEaseType = InEaseType;
	}
	else
	{
		CurrentCameraDistance = TargetCameraDistance;
		CurrentCamera->GetCameraBoom()->TargetArmLength = TargetCameraDistance;
	}
}

void UCameraModule::StopDoCameraDistance()
{
	CameraDoZoomTime = 0.f;
	CameraDoZoomDuration = 0.f;
	CameraDoZoomDistance = 0.f;
}

void UCameraModule::SetCameraRotationAndDistance(float InYaw, float InPitch, float InDistance, bool bInstant)
{
	SetCameraRotation(InYaw, InPitch, bInstant);
	SetCameraDistance(InDistance, bInstant);
}

void UCameraModule::DoCameraRotationAndDistance(float InYaw, float InPitch, float InDistance, float InDuration, EEaseType InEaseType)
{
	DoCameraRotation(InYaw, InPitch, InDuration, InEaseType);
	DoCameraDistance(InDistance, InDuration, InEaseType);
}

void UCameraModule::SetCameraTransform(FVector InLocation, float InYaw, float InPitch, float InDistance, bool bInstant)
{
	SetCameraLocation(InLocation, bInstant);
	SetCameraRotation(InYaw, InPitch, bInstant);
	SetCameraDistance(InDistance, bInstant);
}

void UCameraModule::DoCameraTransform(FVector InLocation, float InYaw, float InPitch, float InDistance, float InDuration, EEaseType InEaseType)
{
	DoCameraLocation(InLocation, InDuration, InEaseType);
	DoCameraRotation(InYaw, InPitch, InDuration, InEaseType);
	DoCameraDistance(InDistance, InDuration, InEaseType);
}

void UCameraModule::StopDoCameraTransform()
{
	StopDoCameraLocation();
	StopDoCameraRotation();
	StopDoCameraDistance();
}

void UCameraModule::AddCameraRotationInput(float InYaw, float InPitch)
{
	if(!bCameraRotateAble || !bCameraRotateControlAble) return;

	bIsControllingRotate = true;
	GetWorld()->GetTimerManager().SetTimerForNextTick([this]()
	{
		bIsControllingRotate = false;
	});

	SetCameraRotation(TargetCameraRotation.Yaw + InYaw * CameraTurnRate * GetWorld()->GetDeltaSeconds(), TargetCameraRotation.Pitch + InPitch * CameraLookUpRate * GetWorld()->GetDeltaSeconds(), false);
}

void UCameraModule::AddCameraDistanceInput(float InValue)
{
	if(!bCameraZoomAble || !bCameraZoomControlAble) return;

	bIsControllingZoom = true;
	GetWorld()->GetTimerManager().SetTimerForNextTick([this]()
	{
		bIsControllingZoom = false;
	});

	SetCameraDistance(TargetCameraDistance + InValue * CameraZoomRate * 2.f * GetWorld()->GetDeltaSeconds(), false);
}

void UCameraModule::AddCameraMovementInput(FVector InDirection, float InValue)
{
	if(!bCameraMoveAble || !bCameraMoveControlAble) return;

	bIsControllingMove = true;
	GetWorld()->GetTimerManager().SetTimerForNextTick([this]()
	{
		bIsControllingMove = false;
	});

	SetCameraLocation(TargetCameraLocation + InDirection * CameraMoveRate * InValue * GetWorld()->GetDeltaSeconds(), false);
}

void UCameraModule::SetCameraView(const FCameraViewData& InCameraViewData)
{
	if(InCameraViewData.bTrackTarget)
	{
		StartTrackTarget(InCameraViewData.CameraViewTarget.LoadSynchronous(), InCameraViewData.TrackTargetMode, InCameraViewData.CameraViewParams.CameraViewSpace,
			InCameraViewData.CameraViewParams.CameraViewOffset, FVector(-1.f), InCameraViewData.CameraViewParams.CameraViewYaw,
			InCameraViewData.CameraViewParams.CameraViewPitch, InCameraViewData.CameraViewParams.CameraViewDistance, true, InCameraViewData.CameraViewParams.CameraViewMode == ECameraViewMode::Instant);
	}
	else
	{
		SetCameraViewParams(InCameraViewData.CameraViewParams);
	}
}

void UCameraModule::SetCameraViewParams(const FCameraViewParams& InCameraViewParams)
{
	if(ACameraActorBase* CameraActor = InCameraViewParams.CameraViewActor.LoadSynchronous())
	{
		SwitchCamera(CameraActor);
	}

	FVector CameraLocation;
	float CameraYaw;
	float CameraPitch;
	float CameraDistance;
	if(InCameraViewParams.CameraViewSpace == ECameraViewSpace::Local && InCameraViewParams.CameraViewTarget)
	{
		CameraLocation = InCameraViewParams.CameraViewTarget->GetActorLocation() + InCameraViewParams.CameraViewOffset;
		CameraYaw = InCameraViewParams.CameraViewTarget->GetActorRotation().Yaw + InCameraViewParams.CameraViewYaw;
		CameraPitch = InCameraViewParams.CameraViewTarget->GetActorRotation().Pitch + InCameraViewParams.CameraViewPitch;
		CameraDistance = InCameraViewParams.CameraViewDistance;
	}
	else
	{
		CameraLocation = InCameraViewParams.CameraViewOffset;
		CameraYaw = InCameraViewParams.CameraViewYaw;
		CameraPitch = InCameraViewParams.CameraViewPitch;
		CameraDistance = InCameraViewParams.CameraViewDistance;
	}
	switch(InCameraViewParams.CameraViewMode)
	{
		case ECameraViewMode::Instant:
		{
			SetCameraLocation(CameraLocation, true);
			SetCameraRotation(CameraYaw, CameraPitch, true);
			SetCameraDistance(CameraDistance, true);
			break;
		}
		case ECameraViewMode::Smooth:
		{
			SetCameraLocation(CameraLocation, false);
			SetCameraRotation(CameraYaw, CameraPitch, false);
			SetCameraDistance(CameraDistance, false);
			break;
		}
		case ECameraViewMode::Duration:
		{
			DoCameraLocation(CameraLocation, InCameraViewParams.CameraViewDuration, InCameraViewParams.CameraViewEaseType);
			DoCameraRotation(CameraYaw, CameraPitch, InCameraViewParams.CameraViewDuration, InCameraViewParams.CameraViewEaseType);
			DoCameraDistance(CameraDistance, InCameraViewParams.CameraViewDuration, InCameraViewParams.CameraViewEaseType);
			break;
		}
		default: break;
	}

	CachedCameraParams = InCameraViewParams;
}

void UCameraModule::ResetCameraView(bool bUseCachedParams)
{
	if(TrackTargetActor) return;

	if(bUseCachedParams && CachedCameraParams.IsValid())
	{
		SetCameraViewParams(CachedCameraParams);
	}
	else if(DefaultCameraPoint)
	{
		SwitchCameraPoint(DefaultCameraPoint);
	}
}

void UCameraModule::SwitchCameraPoint(ACameraPointBase* InCameraPoint, bool bSetAsDefault)
{
	SetCameraView(InCameraPoint->GetCameraViewData());
	if(bSetAsDefault)
	{
		SetDefaultCameraPoint(InCameraPoint);
	}
}

void UCameraModule::OnSetCameraView(UObject* InSender, UEventHandle_SetCameraView* InEventHandle)
{
	SetCameraView(InEventHandle->CameraViewData);
}

void UCameraModule::OnResetCameraView(UObject* InSender, UEventHandle_ResetCameraView* InEventHandle)
{
	ResetCameraView();
}

void UCameraModule::OnSwitchCameraPoint(UObject* InSender, UEventHandle_SwitchCameraPoint* InEventHandle)
{
	SwitchCameraPoint(InEventHandle->CameraPoint);
}

bool UCameraModule::IsControllingMove()
{
	return UInputModuleStatics::GetKeyShortcutByName(FName("CameraPanMove")).IsPressing(GetPlayerController()) || bIsControllingMove;
}

bool UCameraModule::IsControllingRotate()
{
	return UInputModuleStatics::GetKeyShortcutByName(FName("CameraRotate")).IsPressing(GetPlayerController()) || UInputModuleStatics::GetTouchPressedCount() == 1 || bIsControllingRotate;
}

bool UCameraModule::IsControllingZoom()
{
	return bIsControllingZoom;
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

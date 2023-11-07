// Fill out your copyright notice in the Description page of Project Settings.


#include "Camera/CameraModule.h"

#include "Camera/CameraModuleNetworkComponent.h"
#include "Camera/Base/CameraManagerBase.h"
#include "Camera/Roam/RoamCameraActor.h"
#include "GameFramework/SpringArmComponent.h"
#include "Gameplay/WHGameMode.h"
#include "Gameplay/WHPlayerController.h"
#include "Common/CommonBPLibrary.h"
#include "Input/InputModuleBPLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "Main/MainModule.h"
#include "Math/MathBPLibrary.h"
#include "SaveGame/SaveGameModuleBPLibrary.h"
#include "SaveGame/Module/CameraSaveGame.h"

IMPLEMENTATION_MODULE(ACameraModule)

// Sets default values
ACameraModule::ACameraModule()
{
	ModuleName = FName("CameraModule");

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
	CameraMoveRate = 300.f;
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
	CameraZoomRate = 150.f;
	bSmoothCameraZoom = true;
	CameraZoomSpeed = 5.f;
	MinCameraDistance = 0.f;
	MaxCameraDistance = -1.f;
	InitCameraDistance = 0.f;

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
	TrackTargetMode = ETrackTargetMode::LocationOnly;

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

ACameraModule::~ACameraModule()
{
	TERMINATION_MODULE(ACameraModule)
}

#if WITH_EDITOR
void ACameraModule::OnGenerate()
{
	Super::OnGenerate();

	// 获取场景Camera
	TArray<AActor*> ChildActors;
	GetAttachedActors(ChildActors);
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
				CameraPawn->AttachToActor(this, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
				Cameras.Add(CameraPawn);
			}
		}
	}

	Modify();
}

void ACameraModule::OnDestroy()
{
	Super::OnDestroy();

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

void ACameraModule::OnPreparatory_Implementation(EPhase InPhase)
{
	Super::OnPreparatory_Implementation(InPhase);

	if(PHASEC(InPhase, EPhase::Lesser))
	{
		if(bModuleAutoSave)
		{
			Load();
		}
	}

	if(PHASEC(InPhase, EPhase::Final))
	{
		if(DefaultCamera)
		{
			SwitchCamera(DefaultCamera, DefaultInstantSwitch);
		}
	}
}

void ACameraModule::OnRefresh_Implementation(float DeltaSeconds)
{
	Super::OnRefresh_Implementation(DeltaSeconds);

	if(!bCameraControlAble) return;

	DoTrackTarget();

	if(bCameraMoveAble && CurrentCamera)
	{
		CurrentCameraLocation = CurrentCamera->GetActorLocation();
		if(CurrentCameraLocation != TargetCameraLocation)
		{
			if(CameraDoMoveDuration != 0.f)
			{
				CameraDoMoveTime = FMath::Clamp(CameraDoMoveTime + DeltaSeconds, 0.f, CameraDoMoveDuration);
				CurrentCamera->SetCameraLocation(FMath::Lerp(CameraDoMoveLocation, TargetCameraLocation, UMathBPLibrary::EvaluateByEaseType(CameraDoMoveEaseType, CameraDoMoveTime, CameraDoMoveDuration)));
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
				GetPlayerController()->SetControlRotation(UMathBPLibrary::LerpRotator(CameraDoRotateRotation, TargetCameraRotation, UMathBPLibrary::EvaluateByEaseType(CameraDoRotateEaseType, CameraDoRotateTime, CameraDoRotateDuration), !CameraDoRotateRotation.Equals(TargetCameraRotation)));
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
				CurrentCamera->GetCameraBoom()->TargetArmLength = FMath::Lerp(CameraDoZoomDistance, TargetCameraDistance, UMathBPLibrary::EvaluateByEaseType(CameraDoZoomEaseType, CameraDoZoomTime, CameraDoZoomDuration));
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

void ACameraModule::OnPause_Implementation()
{
	Super::OnPause_Implementation();
}

void ACameraModule::OnUnPause_Implementation()
{
	Super::OnUnPause_Implementation();
}

void ACameraModule::OnTermination_Implementation(EPhase InPhase)
{
	Super::OnTermination_Implementation(InPhase);

	if(PHASEC(InPhase, EPhase::Lesser))
	{
		if(bModuleAutoSave)
		{
			Save();
		}
	}
}

void ACameraModule::LoadData(FSaveData* InSaveData, EPhase InPhase)
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

void ACameraModule::UnloadData(EPhase InPhase)
{
}

FSaveData* ACameraModule::ToData()
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

ACameraActorBase* ACameraModule::GetCurrentCamera(TSubclassOf<ACameraActorBase> InClass) const
{
	return CurrentCamera;
}

ACameraManagerBase* ACameraModule::GetCurrentCameraManager()
{
	if(GetPlayerController())
	{
		return Cast<ACameraManagerBase>(GetPlayerController()->PlayerCameraManager);
	}
	return nullptr;
}

ACameraActorBase* ACameraModule::GetCameraByClass(TSubclassOf<ACameraActorBase> InClass)
{
	if(!InClass) return nullptr;
	
	const FName CameraName = InClass.GetDefaultObject()->GetCameraName();
	return GetCameraByName(CameraName);
}

ACameraActorBase* ACameraModule::GetCameraByName(const FName InName) const
{
	if(CameraMap.Contains(InName))
	{
		return CameraMap[InName];
	}
	return nullptr;
}

void ACameraModule::SwitchCamera(ACameraActorBase* InCamera, bool bInstant)
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
			SetCameraDistance(InitCameraDistance);
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

void ACameraModule::SwitchCameraByClass(TSubclassOf<ACameraActorBase> InClass, bool bInstant)
{
	const FName CameraName = InClass ? InClass.GetDefaultObject()->GetCameraName() : NAME_None;
	SwitchCameraByName(CameraName, bInstant);
}

void ACameraModule::SwitchCameraByName(const FName InName, bool bInstant)
{
	SwitchCamera(GetCameraByName(InName), bInstant);
}

void ACameraModule::DoTrackTarget(bool bInstant)
{
	if(!TrackTargetActor) return;
	
	switch(TrackTargetMode)
	{
		case ETrackTargetMode::LocationOnly:
		{
			if(!bTrackAllowControl || !IsControllingMove())
			{
				SetCameraLocation(TrackTargetActor->GetActorLocation() + TrackTargetActor->GetActorRotation().RotateVector(TrackLocationOffset), bInstant);
			}
			break;
		}
		case ETrackTargetMode::LocationAndRotation:
		{
			if(!bTrackAllowControl || !IsControllingMove())
			{
				SetCameraLocation(TrackTargetActor->GetActorLocation() + TrackTargetActor->GetActorRotation().RotateVector(TrackLocationOffset), bInstant);
			}
			if(!bTrackAllowControl || !IsControllingRotate())
			{
				SetCameraRotation(TrackTargetActor->GetActorRotation().Yaw + TrackYawOffset, TrackTargetActor->GetActorRotation().Pitch + TrackPitchOffset, bInstant);
			}
			break;
		}
		case ETrackTargetMode::LocationAndRotationOnce:
		{
			if(!bTrackAllowControl || !IsControllingMove())
			{
				SetCameraLocation(TrackTargetActor->GetActorLocation() + TrackTargetActor->GetActorRotation().RotateVector(TrackLocationOffset), bInstant);
			}
			if(!bTrackAllowControl || !IsControllingRotate())
			{
				SetCameraRotation(TrackTargetActor->GetActorRotation().Yaw + TrackYawOffset, TrackTargetActor->GetActorRotation().Pitch + TrackPitchOffset, bInstant);
			}
			TrackTargetMode = ETrackTargetMode::LocationOnly;
			break;
		}
		case ETrackTargetMode::LocationAndRotationAndDistance:
		{
			if(!bTrackAllowControl || !IsControllingMove())
			{
				SetCameraLocation(TrackTargetActor->GetActorLocation() + TrackTargetActor->GetActorRotation().RotateVector(TrackLocationOffset), bInstant);
			}
			if(!bTrackAllowControl || !IsControllingRotate())
			{
				SetCameraRotation(TrackTargetActor->GetActorRotation().Yaw + TrackYawOffset, TrackTargetActor->GetActorRotation().Pitch + TrackPitchOffset, bInstant);
			}
			if(!bTrackAllowControl || !IsControllingZoom())
			{
				SetCameraDistance(TrackDistance, bInstant);
			}
			break;
		}
		case ETrackTargetMode::LocationAndRotationAndDistanceOnce:
		{
			if(!bTrackAllowControl || !IsControllingMove())
			{
				SetCameraLocation(TrackTargetActor->GetActorLocation() + TrackTargetActor->GetActorRotation().RotateVector(TrackLocationOffset), bInstant);
			}
			if(!bTrackAllowControl || !IsControllingRotate())
			{
				SetCameraRotation(TrackTargetActor->GetActorRotation().Yaw + TrackYawOffset, TrackTargetActor->GetActorRotation().Pitch + TrackPitchOffset, bInstant);
			}
			if(!bTrackAllowControl || !IsControllingZoom())
			{
				SetCameraDistance(TrackDistance, bInstant);
			}
			TrackTargetMode = ETrackTargetMode::LocationAndRotation;
			break;
		}
	}
}

void ACameraModule::StartTrackTarget(AActor* InTargetActor, ETrackTargetMode InTrackTargetMode, ETrackTargetSpace InTrackTargetSpace, FVector InLocationOffset, FVector InSocketOffset, float InYawOffset, float InPitchOffset, float InDistance, bool bAllowControl, bool bInstant)
{
	if(!InTargetActor || !CurrentCamera) return;

	if(TrackTargetActor != InTargetActor)
	{
		TrackTargetActor = InTargetActor;
		TrackTargetMode = InTrackTargetMode;
		TrackLocationOffset = InLocationOffset == FVector(-1.f) ? TargetCameraLocation - InTargetActor->GetActorLocation() : InTrackTargetSpace == ETrackTargetSpace::Local ? InLocationOffset : InLocationOffset - InTargetActor->GetActorLocation();
		CurrentCamera->GetCameraBoom()->SocketOffset = InSocketOffset == FVector(-1.f) ? InitSocketOffset : InSocketOffset;
		TrackYawOffset = InYawOffset == -1.f ? TargetCameraRotation.Yaw - InTargetActor->GetActorRotation().Yaw : InTrackTargetSpace == ETrackTargetSpace::Local ? InYawOffset : InYawOffset - InTargetActor->GetActorRotation().Yaw;
		TrackPitchOffset = InPitchOffset == -1.f ? TargetCameraRotation.Pitch - InTargetActor->GetActorRotation().Pitch : InTrackTargetSpace == ETrackTargetSpace::Local ? InPitchOffset : InPitchOffset - InTargetActor->GetActorRotation().Pitch;
		TrackDistance = InDistance == -1.f ? TargetCameraDistance : InDistance;
		bTrackAllowControl = bAllowControl;
		DoTrackTarget(bInstant);
	}
}

void ACameraModule::EndTrackTarget(AActor* InTargetActor)
{
	if(!CurrentCamera) return;

	if(!InTargetActor || InTargetActor == TrackTargetActor)
	{
		TrackTargetActor = nullptr;
	}
}

void ACameraModule::SetCameraLocation(FVector InLocation, bool bInstant)
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

void ACameraModule::DoCameraLocation(FVector InLocation, float InDuration, EEaseType InEaseType)
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

void ACameraModule::StopDoCameraLocation()
{
	CameraDoMoveTime = 0.f;
	CameraDoMoveDuration = 0.f;
	CameraDoMoveLocation = FVector::ZeroVector;
}

void ACameraModule::SetCameraRotation(float InYaw, float InPitch, bool bInstant)
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

void ACameraModule::DoCameraRotation(float InYaw, float InPitch, float InDuration, EEaseType InEaseType)
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

void ACameraModule::StopDoCameraRotation()
{
	CameraDoRotateTime = 0.f;
	CameraDoRotateDuration = 0.f;
	CameraDoRotateRotation = FRotator::ZeroRotator;
}

void ACameraModule::SetCameraDistance(float InDistance, bool bInstant)
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

void ACameraModule::DoCameraDistance(float InDistance, float InDuration, EEaseType InEaseType)
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

	bIsControllingRotate = true;
	GetWorldTimerManager().SetTimerForNextTick([this]()
	{
		bIsControllingRotate = false;
	});

	SetCameraRotation(TargetCameraRotation.Yaw + InYaw * CameraTurnRate * GetWorld()->GetDeltaSeconds(), TargetCameraRotation.Pitch + InPitch * CameraLookUpRate * GetWorld()->GetDeltaSeconds(), false);
}

void ACameraModule::AddCameraDistanceInput(float InValue)
{
	if(!bCameraZoomAble || !bCameraZoomControlAble) return;

	bIsControllingZoom = true;
	GetWorldTimerManager().SetTimerForNextTick([this]()
	{
		bIsControllingZoom = false;
	});

	SetCameraDistance(TargetCameraDistance + InValue * CameraZoomRate * GetWorld()->GetDeltaSeconds(), false);
}

void ACameraModule::AddCameraMovementInput(FVector InDirection, float InValue)
{
	if(!bCameraMoveAble || !bCameraMoveControlAble) return;

	bIsControllingMove = true;
	GetWorldTimerManager().SetTimerForNextTick([this]()
	{
		bIsControllingMove = false;
	});

	SetCameraLocation(TargetCameraLocation + InDirection * CameraMoveRate * InValue * GetWorld()->GetDeltaSeconds(), false);
}

bool ACameraModule::IsControllingMove()
{
	return UInputModuleBPLibrary::GetKeyShortcutByName(FName("CameraPanMove")).IsPressing(GetPlayerController()) || bIsControllingMove;
}

bool ACameraModule::IsControllingRotate()
{
	return UInputModuleBPLibrary::GetKeyShortcutByName(FName("CameraRotate")).IsPressing(GetPlayerController()) || UInputModuleBPLibrary::GetTouchPressedCount() == 1 || bIsControllingRotate;
}

bool ACameraModule::IsControllingZoom()
{
	return bIsControllingZoom;
}

AWHPlayerController* ACameraModule::GetPlayerController()
{
	if(!PlayerController)
	{
		PlayerController = UCommonBPLibrary::GetPlayerController<AWHPlayerController>();
	}
	return PlayerController;
}

void ACameraModule::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

}

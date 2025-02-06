// Fill out your copyright notice in the Description page of Project Settings.


#include "Camera/CameraModule.h"

#include "Camera/CameraModuleNetworkComponent.h"
#include "Camera/CameraModuleStatics.h"
#include "Camera/Actor/CCTVCameraActor.h"
#include "Camera/Actor/RoamCameraActor.h"
#include "Camera/Manager/CameraManagerBase.h"
#include "Camera/Point/CameraPointBase.h"
#include "Gameplay/WHGameMode.h"
#include "Common/CommonStatics.h"
#include "Event/EventModuleStatics.h"
#include "Event/Handle/Camera/EventHandle_ResetCameraView.h"
#include "Event/Handle/Camera/EventHandle_SetCameraView.h"
#include "Event/Handle/Camera/EventHandle_SwitchCameraPoint.h"
#include "Event/Handle/Camera/EventHandle_CameraTraceEnded.h"
#include "Input/InputModuleStatics.h"
#include "Main/MainModule.h"
#include "SaveGame/SaveGameModuleStatics.h"
#include "SaveGame/Module/CameraSaveGame.h"

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
	bCameraZoomMoveAble = false;
	bNormalizeCameraZoom = false;
	CameraZoomRate = 300.f;
	bSmoothCameraZoom = true;
	CameraZoomSpeed = 5.f;
	MinCameraDistance = 0.f;
	MaxCameraDistance = -1.f;
	InitCameraDistance = -1.f;
	InitCameraFov = 0.f;
	CameraZoomAltitude = 0.f;

	DefaultCameraPoint = nullptr;
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
	
	UEventModuleStatics::SubscribeEvent<UEventHandle_SetCameraView>(this, GET_FUNCTION_NAME_THISCLASS(OnSetCameraView));
	UEventModuleStatics::SubscribeEvent<UEventHandle_ResetCameraView>(this, GET_FUNCTION_NAME_THISCLASS(OnResetCameraView));
	UEventModuleStatics::SubscribeEvent<UEventHandle_SwitchCameraPoint>(this, GET_FUNCTION_NAME_THISCLASS(OnSwitchCameraPoint));
}

void UCameraModule::OnPreparatory(EPhase InPhase)
{
	Super::OnPreparatory(InPhase);
}

void UCameraModule::OnRefresh(float DeltaSeconds, bool bInEditor)
{
	Super::OnRefresh(DeltaSeconds, bInEditor);
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
	return Super::GetModuleDebugMessage();
	// return FString::Printf(TEXT("CurrentCamera: %s"), CurrentCamera ? *CurrentCamera->GetCameraName().ToString() : TEXT("None"));
}

void UCameraModule::OnDrawDebug(UCanvas* InCanvas, APlayerController* InPC)
{
	if(bDrawCameraRange)
	{
		UKismetSystemLibrary::DrawDebugBox(this, CameraMoveRange.GetCenter(), CameraMoveRange.GetExtent(), FLinearColor::Red);
	}
}

ACameraManagerBase* UCameraModule::GetCameraManager(int32 InPlayerIndex) const
{
	if(CameraManagers.IsValidIndex(InPlayerIndex))
	{
		return CameraManagers[InPlayerIndex];
	}
	return nullptr;
}

ACameraActorBase* UCameraModule::GetCurrentCamera(int32 InPlayerIndex, TSubclassOf<ACameraActorBase> InClass) const
{
	if(ACameraManagerBase* CameraManager = GetCameraManager(InPlayerIndex))
	{
		return GetDeterminesOutputObject(CameraManager->GetCurrentCamera(), InClass);
	}
	return nullptr;
}

ACameraActorBase* UCameraModule::GetCameraByClass(TSubclassOf<ACameraActorBase> InClass, int32 InPlayerIndex)
{
	if(ACameraManagerBase* CameraManager = GetCameraManager(InPlayerIndex))
	{
		return CameraManager->GetCameraByClass(InClass);
	}
	return nullptr;
}

ACameraActorBase* UCameraModule::GetCameraByName(const FName InName, int32 InPlayerIndex) const
{
	if(ACameraManagerBase* CameraManager = GetCameraManager(InPlayerIndex))
	{
		return CameraManager->GetCameraByName(InName);
	}
	return nullptr;
}

void UCameraModule::SwitchCameraByClass(TSubclassOf<ACameraActorBase> InClass, bool bReset, bool bInstant, int32 InPlayerIndex)
{
	if(ACameraManagerBase* CameraManager = GetCameraManager(InPlayerIndex))
	{
		CameraManager->SwitchCameraByClass(InClass, bReset, bInstant);
	}
}

void UCameraModule::SwitchCameraByName(const FName InName, bool bReset, bool bInstant, int32 InPlayerIndex)
{
	if(ACameraManagerBase* CameraManager = GetCameraManager(InPlayerIndex))
	{
		CameraManager->SwitchCameraByName(InName, bReset, bInstant);
	}
}

void UCameraModule::SwitchCameraPoint(ACameraPointBase* InCameraPoint, bool bSetAsDefault, bool bInstant, int32 InPlayerIndex)
{
	if(ACameraManagerBase* CameraManager = GetCameraManager(InPlayerIndex))
	{
		CameraManager->SwitchCameraPoint(InCameraPoint, bSetAsDefault, bInstant);
	}
}

void UCameraModule::RegisterCameraManager(ACameraManagerBase* InCameraManager)
{
	if(CameraManagers.Contains(InCameraManager)) return;

	InCameraManager->LocalPlayerIndex = CameraManagers.Add(InCameraManager);

	InCameraManager->InitCameraPitch = InitCameraPitch;
	InCameraManager->InitCameraDistance = InitCameraDistance;
	InCameraManager->InitCameraOffset = InitCameraOffset;
	InCameraManager->InitCameraFov = InitCameraFov;
	
	if(InCameraManager->LocalPlayerIndex == 0)
	{
		InCameraManager->Cameras = Cameras;
		if(DefaultCamera)
		{
			InCameraManager->SwitchCamera(DefaultCamera, true, bDefaultInstantSwitch);
		}
	}
	else
	{
		for(auto Iter : Cameras)
		{
			ACameraActorBase* CameraActor = DuplicateObject(Iter, nullptr);
			
			FActorSpawnParameters SpawnInfo;
			SpawnInfo.Template = CameraActor;
			SpawnInfo.bDeferConstruction = true;
			SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

			CameraActor->DestroyConstructedComponents();

			FTransform SpawnTransform;

			if(USceneComponent* RootComponent = CameraActor->GetRootComponent())
			{
				SpawnTransform.SetTranslation(RootComponent->GetRelativeLocation());
				SpawnTransform.SetRotation(RootComponent->GetRelativeRotation().Quaternion());
				SpawnTransform.SetScale3D(RootComponent->GetRelativeScale3D());
			}

			if(ACameraActorBase* SpawnedActor = GetWorld()->SpawnActorAbsolute<ACameraActorBase>(CameraActor->GetClass(), SpawnTransform, SpawnInfo))
			{
#if WITH_EDITOR
				SpawnedActor->SetActorLabel(FString::Printf(TEXT("%s_%d"), *Iter->GetActorLabel(), InCameraManager->LocalPlayerIndex));
#endif
				InCameraManager->Cameras.Add(SpawnedActor);
				if(Iter == DefaultCamera)
				{
					InCameraManager->SwitchCamera(SpawnedActor, true, bDefaultInstantSwitch);
				}
			}
		}
	}
	
	if(DefaultCameraPoint)
	{
		InCameraManager->SwitchCameraPoint(DefaultCameraPoint, true);
	}
}

void UCameraModule::UnRegisterCameraManager(ACameraManagerBase* InCameraManager)
{
	if(!CameraManagers.Contains(InCameraManager)) return;

	for(auto Iter : InCameraManager->Cameras)
	{
		Iter->Destroy();
	}
	
	CameraManagers.Remove(InCameraManager);
}

void UCameraModule::StartTrackTarget(AActor* InTargetActor, ECameraTrackMode InTrackMode, ECameraViewMode InViewMode, ECameraViewSpace InViewSpace, FVector InLocation, FVector InOffset, float InYaw, float InPitch, float InDistance, bool bAllowControl, EEaseType InViewEaseType, float InViewDuration, bool bInstant, int32 InPlayerIndex)
{
	if(ACameraManagerBase* CameraManager = GetCameraManager(InPlayerIndex))
	{
		CameraManager->StartTrackTarget(InTargetActor, InTrackMode, InViewMode, InViewSpace, InLocation, InOffset, InYaw, InPitch, InDistance, bAllowControl, InViewEaseType, InViewDuration, bInstant);
	}
}

void UCameraModule::EndTrackTarget(AActor* InTargetActor, int32 InPlayerIndex)
{
	if(ACameraManagerBase* CameraManager = GetCameraManager(InPlayerIndex))
	{
		CameraManager->EndTrackTarget(InTargetActor);
	}
}

void UCameraModule::SetCameraLocation(FVector InLocation, bool bInstant, int32 InPlayerIndex)
{
	if(ACameraManagerBase* CameraManager = GetCameraManager(InPlayerIndex))
	{
		CameraManager->SetCameraLocation(InLocation, bInstant);
	}
}

void UCameraModule::DoCameraLocation(FVector InLocation, float InDuration, EEaseType InEaseType, bool bForce, int32 InPlayerIndex)
{
	if(ACameraManagerBase* CameraManager = GetCameraManager(InPlayerIndex))
	{
		CameraManager->DoCameraLocation(InLocation, InDuration, InEaseType, bForce);
	}
}

void UCameraModule::StopDoCameraLocation(int32 InPlayerIndex)
{
	if(ACameraManagerBase* CameraManager = GetCameraManager(InPlayerIndex))
	{
		CameraManager->StopDoCameraLocation();
	}
}

void UCameraModule::SetCameraOffset(FVector InOffset, bool bInstant, int32 InPlayerIndex)
{
	if(ACameraManagerBase* CameraManager = GetCameraManager(InPlayerIndex))
	{
		CameraManager->SetCameraOffset(InOffset, bInstant);
	}
}

void UCameraModule::DoCameraOffset(FVector InOffset, float InDuration, EEaseType InEaseType, bool bForce, int32 InPlayerIndex)
{
	if(ACameraManagerBase* CameraManager = GetCameraManager(InPlayerIndex))
	{
		CameraManager->DoCameraOffset(InOffset, InDuration, InEaseType, bForce);
	}
}

void UCameraModule::StopDoCameraOffset(int32 InPlayerIndex)
{
	if(ACameraManagerBase* CameraManager = GetCameraManager(InPlayerIndex))
	{
		CameraManager->StopDoCameraOffset();
	}
}

void UCameraModule::SetCameraRotation(float InYaw, float InPitch, bool bInstant, int32 InPlayerIndex)
{
	if(ACameraManagerBase* CameraManager = GetCameraManager(InPlayerIndex))
	{
		CameraManager->SetCameraRotation(InYaw, InPitch, bInstant);
	}
}

void UCameraModule::DoCameraRotation(float InYaw, float InPitch, float InDuration, EEaseType InEaseType, bool bForce, int32 InPlayerIndex)
{
	if(ACameraManagerBase* CameraManager = GetCameraManager(InPlayerIndex))
	{
		CameraManager->DoCameraRotation(InYaw, InPitch, InDuration, InEaseType, bForce);
	}
}

void UCameraModule::StopDoCameraRotation(int32 InPlayerIndex)
{
	if(ACameraManagerBase* CameraManager = GetCameraManager(InPlayerIndex))
	{
		CameraManager->StopDoCameraRotation();
	}
}

void UCameraModule::SetCameraDistance(float InDistance, bool bInstant, int32 InPlayerIndex)
{
	if(ACameraManagerBase* CameraManager = GetCameraManager(InPlayerIndex))
	{
		CameraManager->SetCameraDistance(InDistance, bInstant);
	}
}

void UCameraModule::DoCameraDistance(float InDistance, float InDuration, EEaseType InEaseType, bool bForce, int32 InPlayerIndex)
{
	if(ACameraManagerBase* CameraManager = GetCameraManager(InPlayerIndex))
	{
		CameraManager->DoCameraDistance(InDistance, InDuration, InEaseType, bForce);
	}
}

void UCameraModule::StopDoCameraDistance(int32 InPlayerIndex)
{
	if(ACameraManagerBase* CameraManager = GetCameraManager(InPlayerIndex))
	{
		CameraManager->StopDoCameraDistance();
	}
}

void UCameraModule::SetCameraRotationAndDistance(float InYaw, float InPitch, float InDistance, bool bInstant, int32 InPlayerIndex)
{
	if(ACameraManagerBase* CameraManager = GetCameraManager(InPlayerIndex))
	{
		CameraManager->SetCameraRotationAndDistance(InYaw, InPitch, InDistance, bInstant);
	}
}

void UCameraModule::DoCameraRotationAndDistance(float InYaw, float InPitch, float InDistance, float InDuration, EEaseType InEaseType, bool bForce, int32 InPlayerIndex)
{
	if(ACameraManagerBase* CameraManager = GetCameraManager(InPlayerIndex))
	{
		CameraManager->DoCameraRotationAndDistance(InYaw, InPitch, InDistance, InDuration, InEaseType, bForce);
	}
}

void UCameraModule::SetCameraTransform(FVector InLocation, float InYaw, float InPitch, float InDistance, bool bInstant, int32 InPlayerIndex)
{
	if(ACameraManagerBase* CameraManager = GetCameraManager(InPlayerIndex))
	{
		CameraManager->SetCameraTransform(InLocation, InYaw, InPitch, InDistance, bInstant);
	}
}

void UCameraModule::DoCameraTransform(FVector InLocation, float InYaw, float InPitch, float InDistance, float InDuration, EEaseType InEaseType, bool bForce, int32 InPlayerIndex)
{
	if(ACameraManagerBase* CameraManager = GetCameraManager(InPlayerIndex))
	{
		CameraManager->DoCameraTransform(InLocation, InYaw, InPitch, InDistance, InDuration, InEaseType, bForce);
	}
}

void UCameraModule::StopDoCameraTransform(int32 InPlayerIndex)
{
	if(ACameraManagerBase* CameraManager = GetCameraManager(InPlayerIndex))
	{
		CameraManager->StopDoCameraTransform();
	}
}

void UCameraModule::SetCameraFov(float InFov, bool bInstant, int32 InPlayerIndex)
{
	if(ACameraManagerBase* CameraManager = GetCameraManager(InPlayerIndex))
	{
		CameraManager->SetCameraFov(InFov, bInstant);
	}
}

void UCameraModule::DoCameraFov(float InFov, float InDuration, EEaseType InEaseType, bool bForce, int32 InPlayerIndex)
{
	if(ACameraManagerBase* CameraManager = GetCameraManager(InPlayerIndex))
	{
		CameraManager->DoCameraFov(InFov, InDuration, InEaseType, bForce);
	}
}

void UCameraModule::StopDoCameraFov(int32 InPlayerIndex)
{
	if(ACameraManagerBase* CameraManager = GetCameraManager(InPlayerIndex))
	{
		CameraManager->StopDoCameraFov();
	}
}

void UCameraModule::AddCameraMovementInput(FVector InDirection, float InValue, int32 InPlayerIndex)
{
	if(ACameraManagerBase* CameraManager = GetCameraManager(InPlayerIndex))
	{
		CameraManager->AddCameraMovementInput(InDirection, InValue);
	}
}

void UCameraModule::AddCameraRotationInput(float InYaw, float InPitch, int32 InPlayerIndex)
{
	if(ACameraManagerBase* CameraManager = GetCameraManager(InPlayerIndex))
	{
		CameraManager->AddCameraRotationInput(InYaw, InPitch);
	}
}

void UCameraModule::AddCameraDistanceInput(float InValue, int32 InPlayerIndex)
{
	if(ACameraManagerBase* CameraManager = GetCameraManager(InPlayerIndex))
	{
		CameraManager->AddCameraDistanceInput(InValue);
	}
}

void UCameraModule::SetCameraView(const FCameraViewData& InCameraViewData, bool bCacheData, bool bInstant, int32 InPlayerIndex)
{
	if(ACameraManagerBase* CameraManager = GetCameraManager(InPlayerIndex))
	{
		CameraManager->SetCameraView(InCameraViewData, bCacheData, bInstant);
	}
}

void UCameraModule::ResetCameraView(ECameraResetMode InCameraResetMode, bool bInstant, int32 InPlayerIndex)
{
	if(ACameraManagerBase* CameraManager = GetCameraManager(InPlayerIndex))
	{
		CameraManager->ResetCameraView(InCameraResetMode, bInstant);
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

bool UCameraModule::IsControllingMove(int32 InPlayerIndex)
{
	if(ACameraManagerBase* CameraManager = GetCameraManager(InPlayerIndex))
	{
		return UInputModuleStatics::GetKeyShortcut(GameplayTags::Input_CameraPanMove).IsPressing(CameraManager->GetOwningPlayerController()) || UInputModuleStatics::GetTouchPressedCount() == 3;
	}
	return false;
}

bool UCameraModule::IsControllingRotate(int32 InPlayerIndex)
{
	if(ACameraManagerBase* CameraManager = GetCameraManager(InPlayerIndex))
	{
		return UInputModuleStatics::GetKeyShortcut(GameplayTags::Input_CameraZoom).IsPressing(CameraManager->GetOwningPlayerController()) || UInputModuleStatics::GetTouchPressedCount() == 2;
	}
	return false;
}

bool UCameraModule::IsControllingZoom(int32 InPlayerIndex)
{
	if(ACameraManagerBase* CameraManager = GetCameraManager(InPlayerIndex))
	{
		return UInputModuleStatics::GetKeyShortcut(GameplayTags::Input_CameraZoom).IsPressing(CameraManager->GetOwningPlayerController()) || UInputModuleStatics::GetTouchPressedCount() == 2;
	}
	return false;
}

FVector UCameraModule::GetRealCameraLocation(int32 InPlayerIndex)
{
	if(ACameraManagerBase* CameraManager = GetCameraManager(InPlayerIndex))
	{
		return CameraManager->GetRealCameraLocation();
	}
	return FVector::ZeroVector;
}

FVector UCameraModule::GetCurrentCameraLocation(bool bRefresh, int32 InPlayerIndex) const
{
	if(ACameraManagerBase* CameraManager = GetCameraManager(InPlayerIndex))
	{
		return CameraManager->GetCurrentCameraLocation(bRefresh);
	}
	return FVector::ZeroVector;
}

FVector UCameraModule::GetRealCameraOffset(int32 InPlayerIndex) const
{
	if(ACameraManagerBase* CameraManager = GetCameraManager(InPlayerIndex))
	{
		return CameraManager->GetRealCameraOffset();
	}
	return FVector::ZeroVector;
}

FVector UCameraModule::GetCurrentCameraOffset(bool bRefresh, int32 InPlayerIndex) const
{
	if(ACameraManagerBase* CameraManager = GetCameraManager(InPlayerIndex))
	{
		return CameraManager->GetCurrentCameraOffset(bRefresh);
	}
	return FVector::ZeroVector;
}

FRotator UCameraModule::GetRealCameraRotation(int32 InPlayerIndex)
{
	if(ACameraManagerBase* CameraManager = GetCameraManager(InPlayerIndex))
	{
		return CameraManager->GetRealCameraRotation();
	}
	return FRotator::ZeroRotator;
}

FRotator UCameraModule::GetCurrentCameraRotation(bool bRefresh, int32 InPlayerIndex)
{
	if(ACameraManagerBase* CameraManager = GetCameraManager(InPlayerIndex))
	{
		return CameraManager->GetCurrentCameraRotation(bRefresh);
	}
	return FRotator::ZeroRotator;
}

float UCameraModule::GetRealCameraDistance(int32 InPlayerIndex) const
{
	if(ACameraManagerBase* CameraManager = GetCameraManager(InPlayerIndex))
	{
		return CameraManager->GetRealCameraDistance();
	}
	return 0.f;
}

float UCameraModule::GetCurrentCameraDistance(bool bRefresh, int32 InPlayerIndex) const
{
	if(ACameraManagerBase* CameraManager = GetCameraManager(InPlayerIndex))
	{
		return CameraManager->GetCurrentCameraDistance(bRefresh);
	}
	return 0.f;
}

float UCameraModule::GetRealCameraFov(int32 InPlayerIndex) const
{
	if(ACameraManagerBase* CameraManager = GetCameraManager(InPlayerIndex))
	{
		return CameraManager->GetRealCameraFov();
	}
	return 0.f;
}

float UCameraModule::GetCurrentCameraFov(bool bRefresh, int32 InPlayerIndex) const
{
	if(ACameraManagerBase* CameraManager = GetCameraManager(InPlayerIndex))
	{
		return CameraManager->GetCurrentCameraFov(bRefresh);
	}
	return 0.f;
}

void UCameraModule::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
}

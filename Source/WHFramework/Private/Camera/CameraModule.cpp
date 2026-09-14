// Fill out your copyright notice in the Description page of Project Settings.


#include "Camera/CameraModule.h"

#include "Camera/CameraModuleNetworkComponent.h"
#include "Camera/CameraModuleStatics.h"
#include "Camera/Actor/CCTVCameraActor.h"
#include "Camera/Actor/RoamCameraActor.h"
#include "Camera/Manager/CameraManagerBase.h"
#include "Camera/Point/CameraPointBase.h"
#include "Gameplay/WHGameMode.h"
#include "Common/CommonModuleStatics.h"
#include "Event/EventModuleStatics.h"
#include "Event/Events/Camera/Event_ResetCameraView.h"
#include "Event/Events/Camera/Event_SetCameraView.h"
#include "Event/Events/Camera/Event_SwitchCameraPoint.h"
#include "Event/Events/Camera/Event_CameraTraceEnded.h"
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
	CameraMoveRange = FBox(EForceInit::ForceInitToZero);

#if WITH_EDITORONLY_DATA
	bDrawCameraRange = false;
#endif

	CameraMoveAltitude = 0.f;

	bCameraOffsetAble = true;
	bSmoothCameraOffset = true;
	CameraOffsetSpeed = 5.f;
	InitCameraOffset = FVector::ZeroVector;

	bCameraRotateAble = true;
	bCameraRotateControlAble = true;
	MinCameraPitch = -90.f;
	MaxCameraPitch = 90.f;
	InitCameraPitch = -1.f;

	bCameraZoomAble = true;
	bCameraZoomControlAble = true;
	bCameraZoomMoveAble = false;
	bNormalizeCameraZoom = false;
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
	
	UEventModuleStatics::SubscribeEvent<FEventSetCameraView>(this, &ThisClass::OnSetCameraView);
	UEventModuleStatics::SubscribeEvent<FEventResetCameraView>(this, &ThisClass::OnResetCameraView);
	UEventModuleStatics::SubscribeEvent<FEventSwitchCameraPoint>(this, &ThisClass::OnSwitchCameraPoint);
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

	if(PHASEC(InPhase, EPhase::Primary))
	{
		IDebuggerInterface::UnRegister();
	}
}

void UCameraModule::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	if(!InSaveData) return;

	const FCameraModuleSaveData& SaveData = InSaveData->CastRef<FCameraModuleSaveData>();
	ApplySettings(SaveData.ToSettings());
}

void UCameraModule::UnloadData(EPhase InPhase)
{
}

FSaveData* UCameraModule::ToData()
{
	FCameraModuleSaveData& SaveData = GetMutableSaveData<FCameraModuleSaveData>();
	SaveData = FCameraModuleSaveData();
	SaveData.FromSettings(CameraSettings);
	
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
	if(!InCameraManager || InCameraManager->LocalPlayerIndex == INDEX_NONE || CameraManagers.Contains(InCameraManager)) return;

	if(CameraManagers.Num() <= InCameraManager->LocalPlayerIndex)
	{
		CameraManagers.SetNum(InCameraManager->LocalPlayerIndex + 1);
	}
	CameraManagers[InCameraManager->LocalPlayerIndex] = InCameraManager;
	InCameraManager->ApplySettings(CameraSettings);

	InCameraManager->InitCameraPitch = InitCameraPitch;
	InCameraManager->InitCameraDistance = InitCameraDistance;
	InCameraManager->InitCameraOffset = InitCameraOffset;
	InCameraManager->InitCameraFov = InitCameraFov;
	
	if(InCameraManager->LocalPlayerIndex == 0)
	{
		InCameraManager->bOwnsRuntimeCameras = false;
		InCameraManager->Cameras = Cameras;
		if(DefaultCamera)
		{
			InCameraManager->SwitchCamera(DefaultCamera, true, bDefaultInstantSwitch);
		}
	}
	else
	{
		InCameraManager->bOwnsRuntimeCameras = true;
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

	if(InCameraManager->bOwnsRuntimeCameras)
	{
		for(auto Iter : InCameraManager->Cameras)
		{
			Iter->Destroy();
		}
	}
	
	CameraManagers[InCameraManager->LocalPlayerIndex] = nullptr;
}

void UCameraModule::ApplySettings(const FCameraSettings& InSettings)
{
	CameraSettings = InSettings;

	for(ACameraManagerBase* CameraManager : CameraManagers)
	{
		if(CameraManager)
		{
			CameraManager->ApplySettings(CameraSettings);
		}
	}
}

void UCameraModule::OnSetCameraView(UObject* InSender, const FEventSetCameraView& InEvent)
{
	if(ACameraManagerBase* CameraManager = GetCameraManager())
	{
		CameraManager->ApplyViewData(InEvent.CameraViewData, InEvent.bCacheData);
	}
}

void UCameraModule::OnResetCameraView(UObject* InSender, const FEventResetCameraView& InEvent)
{
	if(ACameraManagerBase* CameraManager = GetCameraManager())
	{
		CameraManager->ResetView(InEvent.CameraResetMode);
	}
}

void UCameraModule::OnSwitchCameraPoint(UObject* InSender, const FEventSwitchCameraPoint& InEvent)
{
	SwitchCameraPoint(InEvent.CameraPoint.LoadSynchronous());
}

void UCameraModule::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
}

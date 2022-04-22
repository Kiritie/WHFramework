// Fill out your copyright notice in the Description page of Project Settings.


#include "Camera/CameraModule.h"

#include "Camera/Roam/RoamCameraPawn.h"
#include "Gameplay/WHGameMode.h"
#include "Gameplay/WHPlayerController.h"
#include "Global/GlobalBPLibrary.h"
#include "Main/MainModule.h"
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
}

void ACameraModule::OnPause_Implementation()
{
	Super::OnPause_Implementation();
}

void ACameraModule::OnUnPause_Implementation()
{
	Super::OnUnPause_Implementation();
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
			if(AWHPlayerController* PlayerController = UGlobalBPLibrary::GetPlayerController<AWHPlayerController>(this))
			{
				PlayerController->Possess(Camera);
				
				PlayerController->SetCameraLocation(Camera->GetActorLocation(), bInstant);
				PlayerController->SetCameraRotation(Camera->GetActorRotation().Yaw, -1, bInstant);
				PlayerController->SetCameraDistance(-1, bInstant);
			}
			CurrentCamera = Camera;
		}
	}
}

void ACameraModule::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

}

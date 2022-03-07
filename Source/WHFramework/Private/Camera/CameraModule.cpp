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

	CameraClasses = TArray<TSubclassOf<ACameraPawnBase>>();
	CameraClasses.Add(ARoamCameraPawn::StaticClass());
	CurrentCamera = nullptr;

	Cameras = TArray<ACameraPawnBase*>();
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
				Camera->AttachToActor(AMainModule::GetModuleByClass<ACameraModule>(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
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
				CameraPawn->AttachToActor(AMainModule::GetModuleByClass<ACameraModule>(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
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
}

void ACameraModule::OnPreparatory_Implementation()
{
	Super::OnPreparatory_Implementation();
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

void ACameraModule::SwitchCamera(ACameraPawnBase* InCamera)
{
	if(CurrentCamera != InCamera)
	{
		if(AWHPlayerController* PlayerController = UGlobalBPLibrary::GetPlayerController<AWHPlayerController>(this))
		{
			PlayerController->Possess(InCamera);
		}
		CurrentCamera = InCamera;
	}
}

void ACameraModule::AddCameraToList(ACameraPawnBase* InCamera)
{
	if(!Cameras.Contains(InCamera))
	{
		Cameras.Add(InCamera);
	}
}

void ACameraModule::RemoveCameraFromList(ACameraPawnBase* InCamera)
{
	if(Cameras.Contains(InCamera))
	{
		Cameras.Remove(InCamera);
	}
}

void ACameraModule::RemoveCameraFromListByName(const FName InCameraName)
{
	RemoveCameraFromList(GetCameraByName(InCameraName));
}

ACameraPawnBase* ACameraModule::GetCameraByName(const FName InCameraName) const
{
	for (auto Iter : Cameras)
	{
		if(Iter->GetCameraName() == InCameraName)
		{
			return Iter;
		}
	}
	return nullptr;
}

void ACameraModule::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ACameraModule, Cameras);
}

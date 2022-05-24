// Fill out your copyright notice in the Description page of Project Settings.


#include "Camera/CameraModuleBPLibrary.h"

#include "Kismet/GameplayStatics.h"
#include "Camera/CameraModule.h"
#include "Main/MainModule.h"
#include "Main/MainModuleBPLibrary.h"

ACameraPawnBase* UCameraModuleBPLibrary::K2_GetCurrentCamera(TSubclassOf<ACameraPawnBase> InClass)
{
	if(ACameraModule* CameraModule = AMainModule::GetModuleByClass<ACameraModule>())
	{
		return CameraModule->K2_GetCurrentCamera(InClass);
	}
	return nullptr;
}

ACameraPawnBase* UCameraModuleBPLibrary::K2_GetCamera(TSubclassOf<ACameraPawnBase> InClass)
{
	if(ACameraModule* CameraModule = AMainModule::GetModuleByClass<ACameraModule>())
	{
		return CameraModule->K2_GetCamera(InClass);
	}
	return nullptr;
}

ACameraPawnBase* UCameraModuleBPLibrary::K2_GetCameraByName(const FName InCameraName)
{
	if(ACameraModule* CameraModule = AMainModule::GetModuleByClass<ACameraModule>())
	{
		return CameraModule->K2_GetCameraByName(InCameraName);
	}
	return nullptr;
}

void UCameraModuleBPLibrary::K2_SwitchCamera(TSubclassOf<ACameraPawnBase> InClass)
{
	if(ACameraModule* CameraModule = AMainModule::GetModuleByClass<ACameraModule>())
	{
		CameraModule->K2_SwitchCamera(InClass);
	}
}

void UCameraModuleBPLibrary::SwitchCameraByName(const FName InCameraName)
{
	if(ACameraModule* CameraModule = AMainModule::GetModuleByClass<ACameraModule>())
	{
		CameraModule->SwitchCameraByName(InCameraName);
	}
}

void UCameraModuleBPLibrary::StartTrackTarget(AActor* InTargetActor, ETrackTargetMode InTrackTargetMode, ETrackTargetSpace InTrackTargetSpace, FVector InLocationOffset, float InYawOffset, float InPitchOffset, float InDistance, bool bAllowControl, bool bInstant)
{
	if(ACameraModule* CameraModule = AMainModule::GetModuleByClass<ACameraModule>())
	{
		CameraModule->StartTrackTarget(InTargetActor, InTrackTargetMode, InTrackTargetSpace, InLocationOffset, InYawOffset, InPitchOffset, InDistance, bAllowControl, bInstant);
	}
}

void UCameraModuleBPLibrary::EndTrackTarget(AActor* InTargetActor)
{
	if(ACameraModule* CameraModule = AMainModule::GetModuleByClass<ACameraModule>())
	{
		CameraModule->EndTrackTarget(InTargetActor);
	}
}

void UCameraModuleBPLibrary::SetCameraLocation(FVector InLocation, bool bInstant)
{
	if(ACameraModule* CameraModule = AMainModule::GetModuleByClass<ACameraModule>())
	{
		CameraModule->SetCameraLocation(InLocation, bInstant);
	}
}

void UCameraModuleBPLibrary::DoCameraLocation(FVector InLocation, float InDuration, EEaseType InEaseType)
{
	if(ACameraModule* CameraModule = AMainModule::GetModuleByClass<ACameraModule>())
	{
		CameraModule->DoCameraLocation(InLocation, InDuration, InEaseType);
	}
}

void UCameraModuleBPLibrary::StopDoCameraLocation()
{
	if(ACameraModule* CameraModule = AMainModule::GetModuleByClass<ACameraModule>())
	{
		CameraModule->StopDoCameraLocation();
	}
}

void UCameraModuleBPLibrary::SetCameraRotation(float InYaw, float InPitch, bool bInstant)
{
	if(ACameraModule* CameraModule = AMainModule::GetModuleByClass<ACameraModule>())
	{
		CameraModule->SetCameraRotation(InYaw, InPitch, bInstant);
	}
}

void UCameraModuleBPLibrary::DoCameraRotation(float InYaw, float InPitch, float InDuration, EEaseType InEaseType)
{
	if(ACameraModule* CameraModule = AMainModule::GetModuleByClass<ACameraModule>())
	{
		CameraModule->DoCameraRotation(InYaw, InPitch, InDuration, InEaseType);
	}
}

void UCameraModuleBPLibrary::StopDoCameraRotation()
{
	if(ACameraModule* CameraModule = AMainModule::GetModuleByClass<ACameraModule>())
	{
		CameraModule->StopDoCameraRotation();
	}
}

void UCameraModuleBPLibrary::SetCameraDistance(float InDistance, bool bInstant)
{
	if(ACameraModule* CameraModule = AMainModule::GetModuleByClass<ACameraModule>())
	{
		CameraModule->SetCameraDistance(InDistance, bInstant);
	}
}

void UCameraModuleBPLibrary::DoCameraDistance(float InDistance, float InDuration, EEaseType InEaseType)
{
	if(ACameraModule* CameraModule = AMainModule::GetModuleByClass<ACameraModule>())
	{
		CameraModule->DoCameraDistance(InDistance, InDuration, InEaseType);
	}
}

void UCameraModuleBPLibrary::StopDoCameraDistance()
{
	if(ACameraModule* CameraModule = AMainModule::GetModuleByClass<ACameraModule>())
	{
		CameraModule->StopDoCameraDistance();
	}
}

void UCameraModuleBPLibrary::SetCameraRotationAndDistance(float InYaw, float InPitch, float InDistance, bool bInstant)
{
	if(ACameraModule* CameraModule = AMainModule::GetModuleByClass<ACameraModule>())
	{
		CameraModule->SetCameraRotationAndDistance(InYaw, InPitch, InDistance, bInstant);
	}
}

void UCameraModuleBPLibrary::DoCameraRotationAndDistance(float InYaw, float InPitch, float InDistance, float InDuration, EEaseType InEaseType)
{
	if(ACameraModule* CameraModule = AMainModule::GetModuleByClass<ACameraModule>())
	{
		CameraModule->DoCameraRotationAndDistance(InYaw, InPitch, InDistance, InDuration, InEaseType);
	}
}

void UCameraModuleBPLibrary::SetCameraTransform(FVector InLocation, float InYaw, float InPitch, float InDistance, bool bInstant)
{
	if(ACameraModule* CameraModule = AMainModule::GetModuleByClass<ACameraModule>())
	{
		CameraModule->SetCameraTransform(InLocation, InYaw, InPitch, InDistance, bInstant);
	}
}

void UCameraModuleBPLibrary::DoCameraTransform(FVector InLocation, float InYaw, float InPitch, float InDistance, float InDuration, EEaseType InEaseType)
{
	if(ACameraModule* CameraModule = AMainModule::GetModuleByClass<ACameraModule>())
	{
		CameraModule->DoCameraTransform(InLocation, InYaw, InPitch, InDistance, InDuration, InEaseType);
	}
}

void UCameraModuleBPLibrary::StopDoCameraTransform()
{
	if(ACameraModule* CameraModule = AMainModule::GetModuleByClass<ACameraModule>())
	{
		CameraModule->StopDoCameraTransform();
	}
}

FVector UCameraModuleBPLibrary::GetCurrentCameraLocation()
{
	if(ACameraModule* CameraModule = AMainModule::GetModuleByClass<ACameraModule>())
	{
		return CameraModule->GetCurrentCameraLocation();
	}
	return FVector::ZeroVector;
}

FRotator UCameraModuleBPLibrary::GetCurrentCameraRotation()
{
	if(ACameraModule* CameraModule = AMainModule::GetModuleByClass<ACameraModule>())
	{
		return CameraModule->GetCurrentCameraRotation();
	}
	return FRotator::ZeroRotator;
}
float UCameraModuleBPLibrary::GetCurrentCameraDistance()
{
	if(ACameraModule* CameraModule = AMainModule::GetModuleByClass<ACameraModule>())
	{
		return CameraModule->GetCurrentCameraDistance();
	}
	return 0.f;
}

FVector UCameraModuleBPLibrary::GetTargetCameraLocation()
{
	if(ACameraModule* CameraModule = AMainModule::GetModuleByClass<ACameraModule>())
	{
		return CameraModule->GetTargetCameraLocation();
	}
	return FVector::ZeroVector;
}

FRotator UCameraModuleBPLibrary::GetTargetCameraRotation()
{
	if(ACameraModule* CameraModule = AMainModule::GetModuleByClass<ACameraModule>())
	{
		return CameraModule->GetTargetCameraRotation();
	}
	return FRotator::ZeroRotator;
}

float UCameraModuleBPLibrary::GetTargetCameraDistance()
{
	if(ACameraModule* CameraModule = AMainModule::GetModuleByClass<ACameraModule>())
	{
		return CameraModule->GetTargetCameraDistance();
	}
	return 0.f;
}

FVector UCameraModuleBPLibrary::GetRealCameraLocation()
{
	if(ACameraModule* CameraModule = AMainModule::GetModuleByClass<ACameraModule>())
	{
		return CameraModule->GetRealCameraLocation();
	}
	return FVector::ZeroVector;
}

FRotator UCameraModuleBPLibrary::GetRealCameraRotation()
{
	if(ACameraModule* CameraModule = AMainModule::GetModuleByClass<ACameraModule>())
	{
		return CameraModule->GetRealCameraRotation();
	}
	return FRotator::ZeroRotator;
}

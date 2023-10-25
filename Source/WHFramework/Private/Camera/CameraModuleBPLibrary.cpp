// Fill out your copyright notice in the Description page of Project Settings.


#include "Camera/CameraModuleBPLibrary.h"

#include "Camera/CameraModule.h"
#include "Camera/Base/CameraManagerBase.h"

ACameraActorBase* UCameraModuleBPLibrary::GetCurrentCamera(TSubclassOf<ACameraActorBase> InClass)
{
	if(ACameraModule* CameraModule = ACameraModule::Get())
	{
		return CameraModule->GetCurrentCamera(InClass);
	}
	return nullptr;
}

FVector UCameraModuleBPLibrary::GetCameraLocation(bool bReal)
{
	if(ACameraModule* CameraModule = ACameraModule::Get())
	{
		return bReal ? CameraModule->GetCurrentCameraManager()->GetCameraLocation() : CameraModule->GetCurrentCameraLocation();
	}
	return FVector::ZeroVector;
}

FRotator UCameraModuleBPLibrary::GetCameraRotation(bool bReal)
{
	if(ACameraModule* CameraModule = ACameraModule::Get())
	{
		return bReal ? CameraModule->GetCurrentCameraManager()->GetCameraRotation() : CameraModule->GetCurrentCameraRotation();
	}
	return FRotator::ZeroRotator;
}

float UCameraModuleBPLibrary::GetCameraDistance(bool bReal)
{
	if(ACameraModule* CameraModule = ACameraModule::Get())
	{
		return bReal ? 0.f : CameraModule->GetCurrentCameraDistance();
	}
	return 0.f;
}

ACameraActorBase* UCameraModuleBPLibrary::GetCameraByClass(TSubclassOf<ACameraActorBase> InClass)
{
	if(ACameraModule* CameraModule = ACameraModule::Get())
	{
		return CameraModule->GetCameraByClass(InClass);
	}
	return nullptr;
}

ACameraActorBase* UCameraModuleBPLibrary::GetCameraByName(const FName InName)
{
	if(ACameraModule* CameraModule = ACameraModule::Get())
	{
		return CameraModule->GetCameraByName(InName);
	}
	return nullptr;
}

void UCameraModuleBPLibrary::SwitchCamera(ACameraActorBase* InCamera, bool bInstant)
{
	if(ACameraModule* CameraModule = ACameraModule::Get())
	{
		CameraModule->SwitchCamera(InCamera, bInstant);
	}
}

void UCameraModuleBPLibrary::SwitchCameraByClass(TSubclassOf<ACameraActorBase> InClass, bool bInstant)
{
	if(ACameraModule* CameraModule = ACameraModule::Get())
	{
		CameraModule->SwitchCameraByClass(InClass, bInstant);
	}
}

void UCameraModuleBPLibrary::SwitchCameraByName(const FName InName, bool bInstant)
{
	if(ACameraModule* CameraModule = ACameraModule::Get())
	{
		CameraModule->SwitchCameraByName(InName, bInstant);
	}
}

void UCameraModuleBPLibrary::StartTrackTarget(AActor* InTargetActor, ETrackTargetMode InTrackTargetMode, ETrackTargetSpace InTrackTargetSpace, FVector InLocationOffset, FVector InSocketOffset, float InYawOffset, float InPitchOffset, float InDistance, bool bAllowControl, bool bInstant)
{
	if(ACameraModule* CameraModule = ACameraModule::Get())
	{
		CameraModule->StartTrackTarget(InTargetActor, InTrackTargetMode, InTrackTargetSpace, InLocationOffset, InSocketOffset, InYawOffset, InPitchOffset, InDistance, bAllowControl, bInstant);
	}
}

void UCameraModuleBPLibrary::EndTrackTarget(AActor* InTargetActor)
{
	if(ACameraModule* CameraModule = ACameraModule::Get())
	{
		CameraModule->EndTrackTarget(InTargetActor);
	}
}

void UCameraModuleBPLibrary::SetCameraLocation(FVector InLocation, bool bInstant)
{
	if(ACameraModule* CameraModule = ACameraModule::Get())
	{
		CameraModule->SetCameraLocation(InLocation, bInstant);
	}
}

void UCameraModuleBPLibrary::DoCameraLocation(FVector InLocation, float InDuration, EEaseType InEaseType)
{
	if(ACameraModule* CameraModule = ACameraModule::Get())
	{
		CameraModule->DoCameraLocation(InLocation, InDuration, InEaseType);
	}
}

void UCameraModuleBPLibrary::StopDoCameraLocation()
{
	if(ACameraModule* CameraModule = ACameraModule::Get())
	{
		CameraModule->StopDoCameraLocation();
	}
}

void UCameraModuleBPLibrary::SetCameraRotation(float InYaw, float InPitch, bool bInstant)
{
	if(ACameraModule* CameraModule = ACameraModule::Get())
	{
		CameraModule->SetCameraRotation(InYaw, InPitch, bInstant);
	}
}

void UCameraModuleBPLibrary::DoCameraRotation(float InYaw, float InPitch, float InDuration, EEaseType InEaseType)
{
	if(ACameraModule* CameraModule = ACameraModule::Get())
	{
		CameraModule->DoCameraRotation(InYaw, InPitch, InDuration, InEaseType);
	}
}

void UCameraModuleBPLibrary::StopDoCameraRotation()
{
	if(ACameraModule* CameraModule = ACameraModule::Get())
	{
		CameraModule->StopDoCameraRotation();
	}
}

void UCameraModuleBPLibrary::SetCameraDistance(float InDistance, bool bInstant)
{
	if(ACameraModule* CameraModule = ACameraModule::Get())
	{
		CameraModule->SetCameraDistance(InDistance, bInstant);
	}
}

void UCameraModuleBPLibrary::DoCameraDistance(float InDistance, float InDuration, EEaseType InEaseType)
{
	if(ACameraModule* CameraModule = ACameraModule::Get())
	{
		CameraModule->DoCameraDistance(InDistance, InDuration, InEaseType);
	}
}

void UCameraModuleBPLibrary::StopDoCameraDistance()
{
	if(ACameraModule* CameraModule = ACameraModule::Get())
	{
		CameraModule->StopDoCameraDistance();
	}
}

void UCameraModuleBPLibrary::SetCameraRotationAndDistance(float InYaw, float InPitch, float InDistance, bool bInstant)
{
	if(ACameraModule* CameraModule = ACameraModule::Get())
	{
		CameraModule->SetCameraRotationAndDistance(InYaw, InPitch, InDistance, bInstant);
	}
}

void UCameraModuleBPLibrary::DoCameraRotationAndDistance(float InYaw, float InPitch, float InDistance, float InDuration, EEaseType InEaseType)
{
	if(ACameraModule* CameraModule = ACameraModule::Get())
	{
		CameraModule->DoCameraRotationAndDistance(InYaw, InPitch, InDistance, InDuration, InEaseType);
	}
}

void UCameraModuleBPLibrary::SetCameraTransform(FVector InLocation, float InYaw, float InPitch, float InDistance, bool bInstant)
{
	if(ACameraModule* CameraModule = ACameraModule::Get())
	{
		CameraModule->SetCameraTransform(InLocation, InYaw, InPitch, InDistance, bInstant);
	}
}

void UCameraModuleBPLibrary::DoCameraTransform(FVector InLocation, float InYaw, float InPitch, float InDistance, float InDuration, EEaseType InEaseType)
{
	if(ACameraModule* CameraModule = ACameraModule::Get())
	{
		CameraModule->DoCameraTransform(InLocation, InYaw, InPitch, InDistance, InDuration, InEaseType);
	}
}

void UCameraModuleBPLibrary::StopDoCameraTransform()
{
	if(ACameraModule* CameraModule = ACameraModule::Get())
	{
		CameraModule->StopDoCameraTransform();
	}
}

void UCameraModuleBPLibrary::AddCameraMovementInput(FVector InDirection, float InValue)
{
	if(ACameraModule* CameraModule = ACameraModule::Get())
	{
		CameraModule->AddCameraMovementInput(InDirection, InValue);
	}
}

void UCameraModuleBPLibrary::AddCameraRotationInput(float InYaw, float InPitch)
{
	if(ACameraModule* CameraModule = ACameraModule::Get())
	{
		CameraModule->AddCameraRotationInput(InYaw, InPitch);
	}
}

void UCameraModuleBPLibrary::AddCameraDistanceInput(float InValue)
{
	if(ACameraModule* CameraModule = ACameraModule::Get())
	{
		CameraModule->AddCameraDistanceInput(InValue);
	}
}

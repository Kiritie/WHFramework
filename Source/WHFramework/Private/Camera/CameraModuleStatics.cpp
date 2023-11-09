// Fill out your copyright notice in the Description page of Project Settings.


#include "Camera/CameraModuleStatics.h"

#include "Camera/CameraModule.h"
#include "Camera/Base/CameraManagerBase.h"

ACameraActorBase* UCameraModuleStatics::GetCurrentCamera(TSubclassOf<ACameraActorBase> InClass)
{
	if(UCameraModule* CameraModule = UCameraModule::Get())
	{
		return CameraModule->GetCurrentCamera(InClass);
	}
	return nullptr;
}

FVector UCameraModuleStatics::GetCameraLocation(bool bReal)
{
	if(UCameraModule* CameraModule = UCameraModule::Get())
	{
		return bReal ? CameraModule->GetCurrentCameraManager()->GetCameraLocation() : CameraModule->GetCurrentCameraLocation();
	}
	return FVector::ZeroVector;
}

FRotator UCameraModuleStatics::GetCameraRotation(bool bReal)
{
	if(UCameraModule* CameraModule = UCameraModule::Get())
	{
		return bReal ? CameraModule->GetCurrentCameraManager()->GetCameraRotation() : CameraModule->GetCurrentCameraRotation();
	}
	return FRotator::ZeroRotator;
}

float UCameraModuleStatics::GetCameraDistance(bool bReal)
{
	if(UCameraModule* CameraModule = UCameraModule::Get())
	{
		return bReal ? 0.f : CameraModule->GetCurrentCameraDistance();
	}
	return 0.f;
}

ACameraActorBase* UCameraModuleStatics::GetCameraByClass(TSubclassOf<ACameraActorBase> InClass)
{
	if(UCameraModule* CameraModule = UCameraModule::Get())
	{
		return CameraModule->GetCameraByClass(InClass);
	}
	return nullptr;
}

ACameraActorBase* UCameraModuleStatics::GetCameraByName(const FName InName)
{
	if(UCameraModule* CameraModule = UCameraModule::Get())
	{
		return CameraModule->GetCameraByName(InName);
	}
	return nullptr;
}

void UCameraModuleStatics::SwitchCamera(ACameraActorBase* InCamera, bool bInstant)
{
	if(UCameraModule* CameraModule = UCameraModule::Get())
	{
		CameraModule->SwitchCamera(InCamera, bInstant);
	}
}

void UCameraModuleStatics::SwitchCameraByClass(TSubclassOf<ACameraActorBase> InClass, bool bInstant)
{
	if(UCameraModule* CameraModule = UCameraModule::Get())
	{
		CameraModule->SwitchCameraByClass(InClass, bInstant);
	}
}

void UCameraModuleStatics::SwitchCameraByName(const FName InName, bool bInstant)
{
	if(UCameraModule* CameraModule = UCameraModule::Get())
	{
		CameraModule->SwitchCameraByName(InName, bInstant);
	}
}

void UCameraModuleStatics::StartTrackTarget(AActor* InTargetActor, ETrackTargetMode InTrackTargetMode, ETrackTargetSpace InTrackTargetSpace, FVector InLocationOffset, FVector InSocketOffset, float InYawOffset, float InPitchOffset, float InDistance, bool bAllowControl, bool bInstant)
{
	if(UCameraModule* CameraModule = UCameraModule::Get())
	{
		CameraModule->StartTrackTarget(InTargetActor, InTrackTargetMode, InTrackTargetSpace, InLocationOffset, InSocketOffset, InYawOffset, InPitchOffset, InDistance, bAllowControl, bInstant);
	}
}

void UCameraModuleStatics::EndTrackTarget(AActor* InTargetActor)
{
	if(UCameraModule* CameraModule = UCameraModule::Get())
	{
		CameraModule->EndTrackTarget(InTargetActor);
	}
}

void UCameraModuleStatics::SetCameraLocation(FVector InLocation, bool bInstant)
{
	if(UCameraModule* CameraModule = UCameraModule::Get())
	{
		CameraModule->SetCameraLocation(InLocation, bInstant);
	}
}

void UCameraModuleStatics::DoCameraLocation(FVector InLocation, float InDuration, EEaseType InEaseType)
{
	if(UCameraModule* CameraModule = UCameraModule::Get())
	{
		CameraModule->DoCameraLocation(InLocation, InDuration, InEaseType);
	}
}

void UCameraModuleStatics::StopDoCameraLocation()
{
	if(UCameraModule* CameraModule = UCameraModule::Get())
	{
		CameraModule->StopDoCameraLocation();
	}
}

void UCameraModuleStatics::SetCameraRotation(float InYaw, float InPitch, bool bInstant)
{
	if(UCameraModule* CameraModule = UCameraModule::Get())
	{
		CameraModule->SetCameraRotation(InYaw, InPitch, bInstant);
	}
}

void UCameraModuleStatics::DoCameraRotation(float InYaw, float InPitch, float InDuration, EEaseType InEaseType)
{
	if(UCameraModule* CameraModule = UCameraModule::Get())
	{
		CameraModule->DoCameraRotation(InYaw, InPitch, InDuration, InEaseType);
	}
}

void UCameraModuleStatics::StopDoCameraRotation()
{
	if(UCameraModule* CameraModule = UCameraModule::Get())
	{
		CameraModule->StopDoCameraRotation();
	}
}

void UCameraModuleStatics::SetCameraDistance(float InDistance, bool bInstant)
{
	if(UCameraModule* CameraModule = UCameraModule::Get())
	{
		CameraModule->SetCameraDistance(InDistance, bInstant);
	}
}

void UCameraModuleStatics::DoCameraDistance(float InDistance, float InDuration, EEaseType InEaseType)
{
	if(UCameraModule* CameraModule = UCameraModule::Get())
	{
		CameraModule->DoCameraDistance(InDistance, InDuration, InEaseType);
	}
}

void UCameraModuleStatics::StopDoCameraDistance()
{
	if(UCameraModule* CameraModule = UCameraModule::Get())
	{
		CameraModule->StopDoCameraDistance();
	}
}

void UCameraModuleStatics::SetCameraRotationAndDistance(float InYaw, float InPitch, float InDistance, bool bInstant)
{
	if(UCameraModule* CameraModule = UCameraModule::Get())
	{
		CameraModule->SetCameraRotationAndDistance(InYaw, InPitch, InDistance, bInstant);
	}
}

void UCameraModuleStatics::DoCameraRotationAndDistance(float InYaw, float InPitch, float InDistance, float InDuration, EEaseType InEaseType)
{
	if(UCameraModule* CameraModule = UCameraModule::Get())
	{
		CameraModule->DoCameraRotationAndDistance(InYaw, InPitch, InDistance, InDuration, InEaseType);
	}
}

void UCameraModuleStatics::SetCameraTransform(FVector InLocation, float InYaw, float InPitch, float InDistance, bool bInstant)
{
	if(UCameraModule* CameraModule = UCameraModule::Get())
	{
		CameraModule->SetCameraTransform(InLocation, InYaw, InPitch, InDistance, bInstant);
	}
}

void UCameraModuleStatics::DoCameraTransform(FVector InLocation, float InYaw, float InPitch, float InDistance, float InDuration, EEaseType InEaseType)
{
	if(UCameraModule* CameraModule = UCameraModule::Get())
	{
		CameraModule->DoCameraTransform(InLocation, InYaw, InPitch, InDistance, InDuration, InEaseType);
	}
}

void UCameraModuleStatics::StopDoCameraTransform()
{
	if(UCameraModule* CameraModule = UCameraModule::Get())
	{
		CameraModule->StopDoCameraTransform();
	}
}

void UCameraModuleStatics::AddCameraMovementInput(FVector InDirection, float InValue)
{
	if(UCameraModule* CameraModule = UCameraModule::Get())
	{
		CameraModule->AddCameraMovementInput(InDirection, InValue);
	}
}

void UCameraModuleStatics::AddCameraRotationInput(float InYaw, float InPitch)
{
	if(UCameraModule* CameraModule = UCameraModule::Get())
	{
		CameraModule->AddCameraRotationInput(InYaw, InPitch);
	}
}

void UCameraModuleStatics::AddCameraDistanceInput(float InValue)
{
	if(UCameraModule* CameraModule = UCameraModule::Get())
	{
		CameraModule->AddCameraDistanceInput(InValue);
	}
}

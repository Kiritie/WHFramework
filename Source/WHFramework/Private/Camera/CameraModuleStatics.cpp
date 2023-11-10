// Fill out your copyright notice in the Description page of Project Settings.


#include "Camera/CameraModuleStatics.h"

#include "Camera/CameraModule.h"
#include "Camera/Base/CameraManagerBase.h"

ACameraActorBase* UCameraModuleStatics::GetCurrentCamera(TSubclassOf<ACameraActorBase> InClass)
{
	return UCameraModule::Get().GetCurrentCamera(InClass);
}

FVector UCameraModuleStatics::GetCameraLocation(bool bReal)
{
	return bReal ? UCameraModule::Get().GetCurrentCameraManager()->GetCameraLocation() : UCameraModule::Get().GetCurrentCameraLocation();
}

FRotator UCameraModuleStatics::GetCameraRotation(bool bReal)
{
	return bReal ? UCameraModule::Get().GetCurrentCameraManager()->GetCameraRotation() : UCameraModule::Get().GetCurrentCameraRotation();
}

float UCameraModuleStatics::GetCameraDistance(bool bReal)
{
	return bReal ? 0.f : UCameraModule::Get().GetCurrentCameraDistance();
}

ACameraActorBase* UCameraModuleStatics::GetCameraByClass(TSubclassOf<ACameraActorBase> InClass)
{
	return UCameraModule::Get().GetCameraByClass(InClass);
}

ACameraActorBase* UCameraModuleStatics::GetCameraByName(const FName InName)
{
	return UCameraModule::Get().GetCameraByName(InName);
}

void UCameraModuleStatics::SwitchCamera(ACameraActorBase* InCamera, bool bInstant)
{
	UCameraModule::Get().SwitchCamera(InCamera, bInstant);
}

void UCameraModuleStatics::SwitchCameraByClass(TSubclassOf<ACameraActorBase> InClass, bool bInstant)
{
	UCameraModule::Get().SwitchCameraByClass(InClass, bInstant);
}

void UCameraModuleStatics::SwitchCameraByName(const FName InName, bool bInstant)
{
	UCameraModule::Get().SwitchCameraByName(InName, bInstant);
}

void UCameraModuleStatics::StartTrackTarget(AActor* InTargetActor, ETrackTargetMode InTrackTargetMode, ETrackTargetSpace InTrackTargetSpace, FVector InLocationOffset, FVector InSocketOffset, float InYawOffset, float InPitchOffset, float InDistance, bool bAllowControl, bool bInstant)
{
	UCameraModule::Get().StartTrackTarget(InTargetActor, InTrackTargetMode, InTrackTargetSpace, InLocationOffset, InSocketOffset, InYawOffset, InPitchOffset, InDistance, bAllowControl, bInstant);
}

void UCameraModuleStatics::EndTrackTarget(AActor* InTargetActor)
{
	UCameraModule::Get().EndTrackTarget(InTargetActor);
}

void UCameraModuleStatics::SetCameraLocation(FVector InLocation, bool bInstant)
{
	UCameraModule::Get().SetCameraLocation(InLocation, bInstant);
}

void UCameraModuleStatics::DoCameraLocation(FVector InLocation, float InDuration, EEaseType InEaseType)
{
	UCameraModule::Get().DoCameraLocation(InLocation, InDuration, InEaseType);
}

void UCameraModuleStatics::StopDoCameraLocation()
{
	UCameraModule::Get().StopDoCameraLocation();
}

void UCameraModuleStatics::SetCameraRotation(float InYaw, float InPitch, bool bInstant)
{
	UCameraModule::Get().SetCameraRotation(InYaw, InPitch, bInstant);
}

void UCameraModuleStatics::DoCameraRotation(float InYaw, float InPitch, float InDuration, EEaseType InEaseType)
{
	UCameraModule::Get().DoCameraRotation(InYaw, InPitch, InDuration, InEaseType);
}

void UCameraModuleStatics::StopDoCameraRotation()
{
	UCameraModule::Get().StopDoCameraRotation();
}

void UCameraModuleStatics::SetCameraDistance(float InDistance, bool bInstant)
{
	UCameraModule::Get().SetCameraDistance(InDistance, bInstant);
}

void UCameraModuleStatics::DoCameraDistance(float InDistance, float InDuration, EEaseType InEaseType)
{
	UCameraModule::Get().DoCameraDistance(InDistance, InDuration, InEaseType);
}

void UCameraModuleStatics::StopDoCameraDistance()
{
	UCameraModule::Get().StopDoCameraDistance();
}

void UCameraModuleStatics::SetCameraRotationAndDistance(float InYaw, float InPitch, float InDistance, bool bInstant)
{
	UCameraModule::Get().SetCameraRotationAndDistance(InYaw, InPitch, InDistance, bInstant);
}

void UCameraModuleStatics::DoCameraRotationAndDistance(float InYaw, float InPitch, float InDistance, float InDuration, EEaseType InEaseType)
{
	UCameraModule::Get().DoCameraRotationAndDistance(InYaw, InPitch, InDistance, InDuration, InEaseType);
}

void UCameraModuleStatics::SetCameraTransform(FVector InLocation, float InYaw, float InPitch, float InDistance, bool bInstant)
{
	UCameraModule::Get().SetCameraTransform(InLocation, InYaw, InPitch, InDistance, bInstant);
}

void UCameraModuleStatics::DoCameraTransform(FVector InLocation, float InYaw, float InPitch, float InDistance, float InDuration, EEaseType InEaseType)
{
	UCameraModule::Get().DoCameraTransform(InLocation, InYaw, InPitch, InDistance, InDuration, InEaseType);
}

void UCameraModuleStatics::StopDoCameraTransform()
{
	UCameraModule::Get().StopDoCameraTransform();
}

void UCameraModuleStatics::AddCameraMovementInput(FVector InDirection, float InValue)
{
	UCameraModule::Get().AddCameraMovementInput(InDirection, InValue);
}

void UCameraModuleStatics::AddCameraRotationInput(float InYaw, float InPitch)
{
	UCameraModule::Get().AddCameraRotationInput(InYaw, InPitch);
}

void UCameraModuleStatics::AddCameraDistanceInput(float InValue)
{
	UCameraModule::Get().AddCameraDistanceInput(InValue);
}

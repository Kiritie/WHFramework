// Fill out your copyright notice in the Description page of Project Settings.


#include "Camera/CameraModuleStatics.h"

#include "Camera/CameraModule.h"
#include "Camera/Actor/CameraActorBase.h"
#include "Camera/Manager/CameraManagerBase.h"

ACameraActorBase* UCameraModuleStatics::GetCurrentCamera(TSubclassOf<ACameraActorBase> InClass)
{
	return UCameraModule::Get().GetCurrentCamera(InClass);
}

ACameraPointBase* UCameraModuleStatics::GetDefaultCameraPoint()
{
	return UCameraModule::Get().GetDefaultCameraPoint();
}

void UCameraModuleStatics::SetDefaultCameraPoint(ACameraPointBase* InCameraPoint)
{
	UCameraModule::Get().SetDefaultCameraPoint(InCameraPoint);
}

FVector UCameraModuleStatics::GetCameraLocation(bool bReal, bool bRefresh)
{
	return bReal ? UCameraModule::Get().GetCurrentCameraManager()->GetCameraLocation() : UCameraModule::Get().GetCurrentCameraLocation(bRefresh);
}

FRotator UCameraModuleStatics::GetCameraRotation(bool bRefresh)
{
	return UCameraModule::Get().GetCurrentCameraRotation(bRefresh);
}

float UCameraModuleStatics::GetCameraDistance(bool bRefresh)
{
	return UCameraModule::Get().GetCurrentCameraDistance(bRefresh);
}

FVector UCameraModuleStatics::GetCameraOffset(bool bRefresh)
{
	return UCameraModule::Get().GetCurrentCameraOffset(bRefresh);
}

ACameraActorBase* UCameraModuleStatics::GetCameraByClass(TSubclassOf<ACameraActorBase> InClass)
{
	return UCameraModule::Get().GetCameraByClass(InClass);
}

ACameraActorBase* UCameraModuleStatics::GetCameraByName(const FName InName)
{
	return UCameraModule::Get().GetCameraByName(InName);
}

void UCameraModuleStatics::SwitchCamera(ACameraActorBase* InCamera, bool bReset, bool bInstant)
{
	UCameraModule::Get().SwitchCamera(InCamera, bReset, bInstant);
}

void UCameraModuleStatics::SwitchCameraByClass(TSubclassOf<ACameraActorBase> InClass, bool bReset, bool bInstant)
{
	UCameraModule::Get().SwitchCameraByClass(InClass, bReset, bInstant);
}

void UCameraModuleStatics::SwitchCameraByName(const FName InName, bool bReset, bool bInstant)
{
	UCameraModule::Get().SwitchCameraByName(InName, bReset, bInstant);
}

void UCameraModuleStatics::SwitchCameraPoint(ACameraPointBase* InCameraPoint, bool bSetAsDefault, bool bInstant)
{
	UCameraModule::Get().SwitchCameraPoint(InCameraPoint, bSetAsDefault, bInstant);
}

void UCameraModuleStatics::StartTrackTarget(AActor* InTargetActor, ECameraTrackMode InTrackMode, ECameraViewMode InViewMode, ECameraViewSpace InViewSpace, FVector InLocation, FVector InSocketOffset, float InYaw, float InPitch, float InDistance, bool bAllowControl, EEaseType InViewEaseType, float InViewDuration, bool bInstant)
{
	UCameraModule::Get().StartTrackTarget(InTargetActor, InTrackMode, InViewMode, InViewSpace, InLocation, InSocketOffset, InYaw, InPitch, InDistance, bAllowControl, InViewEaseType, InViewDuration, bInstant);
}

void UCameraModuleStatics::EndTrackTarget(AActor* InTargetActor)
{
	UCameraModule::Get().EndTrackTarget(InTargetActor);
}

void UCameraModuleStatics::SetCameraView(const FCameraViewData& InCameraViewData, bool bCacheData, bool bInstant)
{
	UCameraModule::Get().SetCameraView(InCameraViewData, bCacheData, bInstant);
}

void UCameraModuleStatics::ResetCameraView(ECameraResetMode InCameraResetMode, bool bInstant)
{
	UCameraModule::Get().ResetCameraView(InCameraResetMode, bInstant);
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

void UCameraModuleStatics::SetCameraOffset(FVector InOffset, bool bInstant)
{
	UCameraModule::Get().SetCameraOffset(InOffset, bInstant);
}

void UCameraModuleStatics::DoCameraOffset(FVector InOffset, float InDuration, EEaseType InEaseType)
{
	UCameraModule::Get().DoCameraOffset(InOffset, InDuration, InEaseType);
}

void UCameraModuleStatics::StopDoCameraOffset()
{
	UCameraModule::Get().StopDoCameraOffset();
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

void UCameraModuleStatics::AddCameraDistanceInput(float InValue, bool bMoveIfZero)
{
	UCameraModule::Get().AddCameraDistanceInput(InValue, bMoveIfZero);
}

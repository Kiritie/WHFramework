// Fill out your copyright notice in the Description page of Project Settings.


#include "Camera/CameraModuleStatics.h"

#include "Camera/CameraModule.h"
#include "Camera/Actor/CameraActorBase.h"

ACameraActorBase* UCameraModuleStatics::GetCurrentCamera(int32 InPlayerIndex, TSubclassOf<ACameraActorBase> InClass)
{
	return UCameraModule::Get().GetCurrentCamera(InPlayerIndex, InClass);
}

ACameraPointBase* UCameraModuleStatics::GetDefaultCameraPoint()
{
	return UCameraModule::Get().GetDefaultCameraPoint();
}

FVector UCameraModuleStatics::GetCameraLocation(bool bReally, bool bRefresh, int32 InPlayerIndex)
{
	return bReally ? UCameraModule::Get().GetRealCameraLocation(InPlayerIndex) : UCameraModule::Get().GetCurrentCameraLocation(bRefresh);
}

FRotator UCameraModuleStatics::GetCameraRotation(bool bReally, bool bRefresh, int32 InPlayerIndex)
{
	return bReally ? UCameraModule::Get().GetRealCameraRotation(InPlayerIndex) : UCameraModule::Get().GetCurrentCameraRotation(bRefresh);
}

float UCameraModuleStatics::GetCameraDistance(bool bReally, bool bRefresh, int32 InPlayerIndex)
{
	return bReally ? UCameraModule::Get().GetRealCameraDistance(InPlayerIndex) : UCameraModule::Get().GetCurrentCameraDistance(bRefresh);
}

FVector UCameraModuleStatics::GetCameraOffset(bool bReally, bool bRefresh, int32 InPlayerIndex)
{
	return bReally ? UCameraModule::Get().GetRealCameraOffset(InPlayerIndex) : UCameraModule::Get().GetCurrentCameraOffset(bRefresh);
}

float UCameraModuleStatics::GetCameraFov(bool bReally, bool bRefresh, int32 InPlayerIndex)
{
	return bReally ? UCameraModule::Get().GetRealCameraFov(InPlayerIndex) : UCameraModule::Get().GetCurrentCameraFov(bRefresh);
}

ACameraActorBase* UCameraModuleStatics::GetCameraByClass(TSubclassOf<ACameraActorBase> InClass, int32 InPlayerIndex)
{
	return UCameraModule::Get().GetCameraByClass(InClass, InPlayerIndex);
}

ACameraActorBase* UCameraModuleStatics::GetCameraByName(const FName InName, int32 InPlayerIndex)
{
	return UCameraModule::Get().GetCameraByName(InName, InPlayerIndex);
}

void UCameraModuleStatics::SwitchCameraByClass(TSubclassOf<ACameraActorBase> InClass, bool bReset, bool bInstant, int32 InPlayerIndex)
{
	UCameraModule::Get().SwitchCameraByClass(InClass, bReset, bInstant, InPlayerIndex);
}

void UCameraModuleStatics::SwitchCameraByName(const FName InName, bool bReset, bool bInstant, int32 InPlayerIndex)
{
	UCameraModule::Get().SwitchCameraByName(InName, bReset, bInstant, InPlayerIndex);
}

void UCameraModuleStatics::SwitchCameraPoint(ACameraPointBase* InCameraPoint, bool bSetAsDefault, bool bInstant, int32 InPlayerIndex)
{
	UCameraModule::Get().SwitchCameraPoint(InCameraPoint, bSetAsDefault, bInstant, InPlayerIndex);
}

void UCameraModuleStatics::StartTrackTarget(AActor* InTargetActor, ECameraTrackMode InTrackMode, ECameraViewMode InViewMode, ECameraViewSpace InViewSpace, FVector InLocation, FVector InSocketOffset, float InYaw, float InPitch, float InDistance, bool bAllowControl, EEaseType InViewEaseType, float InViewDuration, bool bInstant, int32 InPlayerIndex)
{
	UCameraModule::Get().StartTrackTarget(InTargetActor, InTrackMode, InViewMode, InViewSpace, InLocation, InSocketOffset, InYaw, InPitch, InDistance, bAllowControl, InViewEaseType, InViewDuration, bInstant, InPlayerIndex);
}

void UCameraModuleStatics::EndTrackTarget(AActor* InTargetActor, int32 InPlayerIndex)
{
	UCameraModule::Get().EndTrackTarget(InTargetActor, InPlayerIndex);
}

void UCameraModuleStatics::SetCameraView(const FCameraViewData& InCameraViewData, bool bCacheData, bool bInstant, int32 InPlayerIndex)
{
	UCameraModule::Get().SetCameraView(InCameraViewData, bCacheData, bInstant, InPlayerIndex);
}

void UCameraModuleStatics::ResetCameraView(ECameraResetMode InCameraResetMode, bool bInstant, int32 InPlayerIndex)
{
	UCameraModule::Get().ResetCameraView(InCameraResetMode, bInstant, InPlayerIndex);
}

void UCameraModuleStatics::SetCameraLocation(FVector InLocation, bool bInstant, int32 InPlayerIndex)
{
	UCameraModule::Get().SetCameraLocation(InLocation, bInstant, InPlayerIndex);
}

void UCameraModuleStatics::DoCameraLocation(FVector InLocation, float InDuration, EEaseType InEaseType, bool bForce, int32 InPlayerIndex)
{
	UCameraModule::Get().DoCameraLocation(InLocation, InDuration, InEaseType, bForce, InPlayerIndex);
}

void UCameraModuleStatics::StopDoCameraLocation(int32 InPlayerIndex)
{
	UCameraModule::Get().StopDoCameraLocation(InPlayerIndex);
}

void UCameraModuleStatics::SetCameraOffset(FVector InOffset, bool bInstant, int32 InPlayerIndex)
{
	UCameraModule::Get().SetCameraOffset(InOffset, bInstant, InPlayerIndex);
}

void UCameraModuleStatics::DoCameraOffset(FVector InOffset, float InDuration, EEaseType InEaseType, bool bForce, int32 InPlayerIndex)
{
	UCameraModule::Get().DoCameraOffset(InOffset, InDuration, InEaseType, bForce, InPlayerIndex);
}

void UCameraModuleStatics::StopDoCameraOffset(int32 InPlayerIndex)
{
	UCameraModule::Get().StopDoCameraOffset(InPlayerIndex);
}

void UCameraModuleStatics::SetCameraRotation(float InYaw, float InPitch, bool bInstant, int32 InPlayerIndex)
{
	UCameraModule::Get().SetCameraRotation(InYaw, InPitch, bInstant, InPlayerIndex);
}

void UCameraModuleStatics::DoCameraRotation(float InYaw, float InPitch, float InDuration, EEaseType InEaseType, bool bForce, int32 InPlayerIndex)
{
	UCameraModule::Get().DoCameraRotation(InYaw, InPitch, InDuration, InEaseType, bForce, InPlayerIndex);
}

void UCameraModuleStatics::StopDoCameraRotation(int32 InPlayerIndex)
{
	UCameraModule::Get().StopDoCameraRotation(InPlayerIndex);
}

void UCameraModuleStatics::SetCameraDistance(float InDistance, bool bInstant, int32 InPlayerIndex)
{
	UCameraModule::Get().SetCameraDistance(InDistance, bInstant, InPlayerIndex);
}

void UCameraModuleStatics::DoCameraDistance(float InDistance, float InDuration, EEaseType InEaseType, bool bForce, int32 InPlayerIndex)
{
	UCameraModule::Get().DoCameraDistance(InDistance, InDuration, InEaseType, bForce, InPlayerIndex);
}

void UCameraModuleStatics::StopDoCameraDistance(int32 InPlayerIndex)
{
	UCameraModule::Get().StopDoCameraDistance(InPlayerIndex);
}

void UCameraModuleStatics::SetCameraRotationAndDistance(float InYaw, float InPitch, float InDistance, bool bInstant, int32 InPlayerIndex)
{
	UCameraModule::Get().SetCameraRotationAndDistance(InYaw, InPitch, InDistance, bInstant, InPlayerIndex);
}

void UCameraModuleStatics::DoCameraRotationAndDistance(float InYaw, float InPitch, float InDistance, float InDuration, EEaseType InEaseType, bool bForce, int32 InPlayerIndex)
{
	UCameraModule::Get().DoCameraRotationAndDistance(InYaw, InPitch, InDistance, InDuration, InEaseType, bForce, InPlayerIndex);
}

void UCameraModuleStatics::SetCameraTransform(FVector InLocation, float InYaw, float InPitch, float InDistance, bool bInstant, int32 InPlayerIndex)
{
	UCameraModule::Get().SetCameraTransform(InLocation, InYaw, InPitch, InDistance, bInstant, InPlayerIndex);
}

void UCameraModuleStatics::DoCameraTransform(FVector InLocation, float InYaw, float InPitch, float InDistance, float InDuration, EEaseType InEaseType, bool bForce, int32 InPlayerIndex)
{
	UCameraModule::Get().DoCameraTransform(InLocation, InYaw, InPitch, InDistance, InDuration, InEaseType, bForce, InPlayerIndex);
}

void UCameraModuleStatics::StopDoCameraTransform(int32 InPlayerIndex)
{
	UCameraModule::Get().StopDoCameraTransform(InPlayerIndex);
}

void UCameraModuleStatics::SetCameraFov(float InFov, bool bInstant, int32 InPlayerIndex)
{
	UCameraModule::Get().SetCameraFov(InFov, bInstant, InPlayerIndex);
}

void UCameraModuleStatics::DoCameraFov(float InFov, float InDuration, EEaseType InEaseType, bool bForce, int32 InPlayerIndex)
{
	UCameraModule::Get().DoCameraFov(InFov, InDuration, InEaseType, bForce, InPlayerIndex);
}

void UCameraModuleStatics::StopDoCameraFov(int32 InPlayerIndex)
{
	UCameraModule::Get().StopDoCameraFov(InPlayerIndex);
}

void UCameraModuleStatics::AddCameraMovementInput(FVector InDirection, float InValue, int32 InPlayerIndex)
{
	UCameraModule::Get().AddCameraMovementInput(InDirection, InValue, InPlayerIndex);
}

void UCameraModuleStatics::AddCameraRotationInput(float InYaw, float InPitch, int32 InPlayerIndex)
{
	UCameraModule::Get().AddCameraRotationInput(InYaw, InPitch, InPlayerIndex);
}

void UCameraModuleStatics::AddCameraDistanceInput(float InValue, int32 InPlayerIndex)
{
	UCameraModule::Get().AddCameraDistanceInput(InValue, InPlayerIndex);
}

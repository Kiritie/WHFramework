// Fill out your copyright notice in the Description page of Project Settings.


#include "Camera/CameraModuleStatics.h"

#include "Camera/CameraModule.h"
#include "Camera/Actor/CameraActorBase.h"
#include "Camera/Manager/CameraManagerBase.h"

ACameraManagerBase* UCameraModuleStatics::GetCameraManager(int32 InPlayerIndex)
{
	return UCameraModule::Get().GetCameraManager(InPlayerIndex);
}

ACameraActorBase* UCameraModuleStatics::GetCurrentCamera(int32 InPlayerIndex, TSubclassOf<ACameraActorBase> InClass)
{
	if(ACameraManagerBase* CameraManager = UCameraModule::Get().GetCameraManager(InPlayerIndex))
	{
		return CameraManager->GetCurrentCamera(InClass);
	}
	return nullptr;
}

ACameraPointBase* UCameraModuleStatics::GetDefaultCameraPoint()
{
	return UCameraModule::Get().GetDefaultCameraPoint();
}

FVector UCameraModuleStatics::GetCameraLocation(bool bReally, bool bRefresh, int32 InPlayerIndex)
{
	if(ACameraManagerBase* CameraManager = UCameraModule::Get().GetCameraManager(InPlayerIndex))
	{
		return bReally ? CameraManager->GetRealCameraLocation() : CameraManager->GetCurrentCameraLocation(bRefresh);
	}
	return FVector::ZeroVector;
}

FRotator UCameraModuleStatics::GetCameraRotation(bool bReally, bool bRefresh, int32 InPlayerIndex)
{
	if(ACameraManagerBase* CameraManager = UCameraModule::Get().GetCameraManager(InPlayerIndex))
	{
		return bReally ? CameraManager->GetRealCameraRotation() : CameraManager->GetCurrentCameraRotation(bRefresh);
	}
	return FRotator::ZeroRotator;
}

float UCameraModuleStatics::GetCameraDistance(bool bReally, bool bRefresh, int32 InPlayerIndex)
{
	if(ACameraManagerBase* CameraManager = UCameraModule::Get().GetCameraManager(InPlayerIndex))
	{
		return bReally ? CameraManager->GetRealCameraDistance() : CameraManager->GetCurrentCameraDistance(bRefresh);
	}
	return 0.f;
}

FVector UCameraModuleStatics::GetCameraOffset(bool bReally, bool bRefresh, int32 InPlayerIndex)
{
	if(ACameraManagerBase* CameraManager = UCameraModule::Get().GetCameraManager(InPlayerIndex))
	{
		return bReally ? CameraManager->GetRealCameraOffset() : CameraManager->GetCurrentCameraOffset(bRefresh);
	}
	return FVector::ZeroVector;
}

float UCameraModuleStatics::GetCameraFov(bool bReally, bool bRefresh, int32 InPlayerIndex)
{
	if(ACameraManagerBase* CameraManager = UCameraModule::Get().GetCameraManager(InPlayerIndex))
	{
		return bReally ? CameraManager->GetRealCameraFov() : CameraManager->GetCurrentCameraFov(bRefresh);
	}
	return 0.f;
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

void UCameraModuleStatics::BindTarget(const FCameraTargetRequest& InRequest, int32 InPlayerIndex)
{
	if(ACameraManagerBase* CameraManager = GetCameraManager(InPlayerIndex))
	{
		CameraManager->BindTarget(InRequest);
	}
}

void UCameraModuleStatics::ClearTarget(AActor* InExpectedTarget, int32 InPlayerIndex)
{
	if(ACameraManagerBase* CameraManager = GetCameraManager(InPlayerIndex))
	{
		CameraManager->ClearTarget(InExpectedTarget);
	}
}

void UCameraModuleStatics::ApplyView(const FCameraViewRequest& InRequest, int32 InPlayerIndex)
{
	if(ACameraManagerBase* CameraManager = GetCameraManager(InPlayerIndex))
	{
		CameraManager->ApplyView(InRequest);
	}
}

void UCameraModuleStatics::ResetView(ECameraResetMode InMode, bool bInstant, int32 InPlayerIndex)
{
	if(ACameraManagerBase* CameraManager = GetCameraManager(InPlayerIndex))
	{
		CameraManager->ResetView(InMode, bInstant);
	}
}

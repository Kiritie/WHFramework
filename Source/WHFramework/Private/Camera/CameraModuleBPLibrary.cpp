// Fill out your copyright notice in the Description page of Project Settings.


#include "Camera/CameraModuleBPLibrary.h"

#include "Kismet/GameplayStatics.h"
#include "Camera/CameraModule.h"
#include "Main/MainModule.h"
#include "Main/MainModuleBPLibrary.h"

void UCameraModuleBPLibrary::AddCameraToList(ACameraPawnBase* InCamera)
{
	if(ACameraModule* CameraModule = AMainModule::GetModuleByClass<ACameraModule>())
	{
		return CameraModule->AddCameraToList(InCamera);
	}
}

void UCameraModuleBPLibrary::RemoveCameraFromList(ACameraPawnBase* InCamera)
{
	if(ACameraModule* CameraModule = AMainModule::GetModuleByClass<ACameraModule>())
	{
		return CameraModule->RemoveCameraFromList(InCamera);
	}
}

void UCameraModuleBPLibrary::RemoveCameraFromListByName(const FName InCameraName)
{
	if(ACameraModule* CameraModule = AMainModule::GetModuleByClass<ACameraModule>())
	{
		return CameraModule->RemoveCameraFromListByName(InCameraName);
	}
}

ACameraPawnBase* UCameraModuleBPLibrary::GetCameraByName(const FName InCameraName)
{
	if(ACameraModule* CameraModule = AMainModule::GetModuleByClass<ACameraModule>())
	{
		return CameraModule->GetCameraByName(InCameraName);
	}
	return nullptr;
}

// Fill out your copyright notice in the Description page of Project Settings.


#include "Camera/CameraModuleBPLibrary.h"

#include "Kismet/GameplayStatics.h"
#include "Camera/CameraModule.h"
#include "Main/MainModule.h"
#include "Main/MainModuleBPLibrary.h"

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

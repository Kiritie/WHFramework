// Copyright Epic Games, Inc. All Rights Reserved.


#include "WHFrameworkCore.h"

#include "Debug/DebugManager.h"
#include "Input/InputManager.h"
#include "Parameter/ParameterManager.h"
#include "Scene/SceneManager.h"
#include "WebRequest/WebRequestManager.h"
#include "Zip/ZipManager.h"
#if PLATFORM_WINDOWS
#include "Platform/Windows/WindowsPlatformManager.h"
#elif PLATFORM_MAC
#include "Platform/Mac/MacPlatformManager.h"
#elif PLATFORM_LINUX
#include "Platform/Linux/LinuxPlatformManager.h"
#endif

void FWHFrameworkCoreModule::StartupModule()
{
	FDebugManager::Register();
	FInputManager::Register();
	FParameterManager::Register();
#if PLATFORM_WINDOWS
	FWindowsPlatformManager::Register();
#elif PLATFORM_MAC
	FMacPlatformManager::Register();
#elif PLATFORM_LINUX
	FLinuxPlatformManager::Register();
#endif
	FSceneManager::Register();
	FWebRequestManager::Register();
	FWebRequestManager::Register();
	FZipManager::Register();
}

void FWHFrameworkCoreModule::ShutdownModule()
{
	FDebugManager::UnRegister();
	FInputManager::UnRegister();
	FParameterManager::UnRegister();
#if PLATFORM_WINDOWS
	FWindowsPlatformManager::UnRegister();
#elif PLATFORM_MAC
	FMacPlatformManager::UnRegister();
#elif PLATFORM_LINUX
	FLinuxPlatformManager::UnRegister();
#endif
	FSceneManager::UnRegister();
	FWebRequestManager::UnRegister();
	FWebRequestManager::UnRegister();
	FZipManager::UnRegister();
}

IMPLEMENT_MODULE(FWHFrameworkCoreModule, WHFrameworkCore)

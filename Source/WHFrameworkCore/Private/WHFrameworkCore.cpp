// Copyright Epic Games, Inc. All Rights Reserved.


#include "WHFrameworkCore.h"

#include "Debug/DebugManager.h"
#include "Input/InputManager.h"
#include "Parameter/ParameterManager.h"
#include "Scene/SceneManager.h"
#include "WebRequest/WebRequestManager.h"
#include "WebRequest/FileDownloader/FileDownloadManager.h"
#include "Zip/ZipManager.h"

void FWHFrameworkCoreModule::StartupModule()
{
	FDebugManager::Register();
	FInputManager::Register();
	FParameterManager::Register();
	FSceneManager::Register();
	FWebRequestManager::Register();
	FFileDownloadManager::Register();
	FZipManager::Register();
}

void FWHFrameworkCoreModule::ShutdownModule()
{
	FDebugManager::UnRegister();
	FInputManager::UnRegister();
	FParameterManager::UnRegister();
	FSceneManager::UnRegister();
	FWebRequestManager::UnRegister();
	FFileDownloadManager::UnRegister();
	FZipManager::UnRegister();
}

IMPLEMENT_MODULE(FWHFrameworkCoreModule, WHFrameworkCore)

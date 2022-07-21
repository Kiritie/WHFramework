// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "SaveGame/General/GeneralSaveGame.h"

#include "Camera/CameraModuleBPLibrary.h"
#include "Global/GlobalBPLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "SaveGame/SaveGameModule.h"
#include "SaveGame/SaveGameModuleBPLibrary.h"

UGeneralSaveGame::UGeneralSaveGame()
{
	SaveName = FName("General");
}

void UGeneralSaveGame::OnCreate_Implementation(int32 InSaveIndex)
{
	Super::OnCreate_Implementation(InSaveIndex);
}

void UGeneralSaveGame::OnLoad_Implementation(bool bForceMode)
{
	Super::OnLoad_Implementation(bForceMode);
}

void UGeneralSaveGame::OnUnload_Implementation(bool bForceMode)
{
	Super::OnUnload_Implementation(bForceMode);
}

void UGeneralSaveGame::OnRefresh_Implementation()
{
	Super::OnRefresh_Implementation();
	
	SetSaveData(AMainModule::GetModuleByClass<ASaveGameModule>()->ToSaveData());
}

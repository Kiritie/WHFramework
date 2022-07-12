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

void UGeneralSaveGame::OnLoad_Implementation()
{
	Super::OnLoad_Implementation();
}

void UGeneralSaveGame::OnUnload_Implementation()
{
	Super::OnLoad_Implementation();
}

void UGeneralSaveGame::OnRefresh_Implementation()
{
	Super::OnRefresh_Implementation();
	
	SetSaveData(USaveGameModuleBPLibrary::ObjectToData(AMainModule::GetModuleByClass<ASaveGameModule>()));
}

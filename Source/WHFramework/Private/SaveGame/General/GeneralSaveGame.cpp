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

void UGeneralSaveGame::OnLoad_Implementation(EPhase InPhase)
{
	Super::OnLoad_Implementation(InPhase);
}

void UGeneralSaveGame::OnUnload_Implementation(EPhase InPhase)
{
	Super::OnUnload_Implementation(InPhase);
}

void UGeneralSaveGame::OnRefresh_Implementation()
{
	Super::OnRefresh_Implementation();
	
	const auto& SaveData = ASaveGameModule::Get()->ToSaveDataRef<FGeneralSaveData>();
	GetSaveDataRef<FGeneralSaveData>().AllSaveGameInfo = SaveData.AllSaveGameInfo;
}

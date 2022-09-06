// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "SaveGame/Parameter/ParameterSaveGame.h"

#include "Camera/CameraModuleBPLibrary.h"
#include "Global/GlobalBPLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Parameter/ParameterModule.h"
#include "SaveGame/SaveGameModuleBPLibrary.h"

UParameterSaveGame::UParameterSaveGame()
{
	SaveName = FName("Parameter");
}

void UParameterSaveGame::OnCreate_Implementation(int32 InSaveIndex)
{
	Super::OnCreate_Implementation(InSaveIndex);
}

void UParameterSaveGame::OnLoad_Implementation(bool bForceMode)
{
	Super::OnLoad_Implementation(bForceMode);
}

void UParameterSaveGame::OnUnload_Implementation(bool bForceMode)
{
	Super::OnUnload_Implementation(bForceMode);
}

void UParameterSaveGame::OnRefresh_Implementation()
{
	Super::OnRefresh_Implementation();
	
	const auto& SaveData = AMainModule::GetModuleByClass<AParameterModule>()->ToSaveDataRef<FParameterSaveData>();
	GetSaveDataRef<FParameterSaveData>().Parameters = SaveData.Parameters;
}

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

void UParameterSaveGame::OnLoad_Implementation(EPhase InPhase)
{
	Super::OnLoad_Implementation(InPhase);
}

void UParameterSaveGame::OnUnload_Implementation(EPhase InPhase)
{
	Super::OnUnload_Implementation(InPhase);
}

void UParameterSaveGame::OnRefresh_Implementation()
{
	Super::OnRefresh_Implementation();
	
	const auto& SaveData = AParameterModule::Get()->ToSaveDataRef<FParameterSaveData>();
	GetSaveDataRef<FParameterSaveData>().Parameters = SaveData.Parameters;
}

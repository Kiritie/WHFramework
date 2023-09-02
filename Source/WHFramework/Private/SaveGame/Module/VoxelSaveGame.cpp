// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "SaveGame/Module/VoxelSaveGame.h"

#include "Camera/CameraModuleBPLibrary.h"
#include "Global/GlobalBPLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Voxel/VoxelModule.h"
#include "SaveGame/SaveGameModuleBPLibrary.h"

UVoxelSaveGame::UVoxelSaveGame()
{
	SaveName = FName("Voxel");
}

void UVoxelSaveGame::OnCreate_Implementation(int32 InIndex)
{
	Super::OnCreate_Implementation(InIndex);

	VoxelSaveData = *AVoxelModule::Get()->NewWorldData();
}

void UVoxelSaveGame::OnLoad_Implementation(EPhase InPhase)
{
	Super::OnLoad_Implementation(InPhase);
}

void UVoxelSaveGame::OnUnload_Implementation(EPhase InPhase)
{
	Super::OnUnload_Implementation(InPhase);
}

void UVoxelSaveGame::OnRefresh_Implementation()
{
	Super::OnRefresh_Implementation();

	SetSaveData(AVoxelModule::Get()->GetSaveData(true));
}

// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "SaveGame/Module/VoxelSaveGame.h"

#include "Voxel/VoxelModule.h"

UVoxelSaveGame::UVoxelSaveGame()
{
	SaveName = FName("Voxel");

	ModuleClass = UVoxelModule::StaticClass();
}

void UVoxelSaveGame::OnCreate_Implementation(int32 InIndex)
{
	SaveData = *UVoxelModule::Get().NewWorldData();
	
	Super::OnCreate_Implementation(InIndex);
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
}

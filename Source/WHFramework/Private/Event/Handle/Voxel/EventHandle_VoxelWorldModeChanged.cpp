// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Voxel/EventHandle_VoxelWorldModeChanged.h"

UEventHandle_VoxelWorldModeChanged::UEventHandle_VoxelWorldModeChanged()
{
	WorldMode = EVoxelWorldMode::None;
}

void UEventHandle_VoxelWorldModeChanged::Parse_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		WorldMode = static_cast<EVoxelWorldMode>(InParams[0].Get<uint8>());
	}
}

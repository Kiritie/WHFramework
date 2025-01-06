// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Voxel/EventHandle_VoxelWorldStateChanged.h"

UEventHandle_VoxelWorldStateChanged::UEventHandle_VoxelWorldStateChanged()
{
	WorldState = EVoxelWorldState::None;
}

void UEventHandle_VoxelWorldStateChanged::Parse_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		WorldState = *InParams[0].GetPointerValue<EVoxelWorldState>();
	}
}

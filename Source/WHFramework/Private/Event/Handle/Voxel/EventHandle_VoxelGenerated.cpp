// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Voxel/EventHandle_VoxelGenerated.h"

UEventHandle_VoxelGenerated::UEventHandle_VoxelGenerated()
{
	VoxelItem = FVoxelItem();
}

void UEventHandle_VoxelGenerated::Parse_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		VoxelItem = *InParams[0].GetPointerValue<FVoxelItem>();
	}
}

// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Voxel/EventHandle_VoxelDestroyed.h"

UEventHandle_VoxelDestroyed::UEventHandle_VoxelDestroyed()
{
	VoxelItem = FVoxelItem();
}

void UEventHandle_VoxelDestroyed::Parse_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		VoxelItem = *InParams[0].GetPointerValue<FVoxelItem>();
	}
}

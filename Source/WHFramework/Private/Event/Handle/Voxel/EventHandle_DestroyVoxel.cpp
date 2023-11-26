// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Voxel/EventHandle_DestroyVoxel.h"

UEventHandle_DestroyVoxel::UEventHandle_DestroyVoxel()
{
	VoxelItem = FVoxelItem();
}

void UEventHandle_DestroyVoxel::Fill_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		VoxelItem = *InParams[0].GetPointerValue<FVoxelItem>();
	}
}

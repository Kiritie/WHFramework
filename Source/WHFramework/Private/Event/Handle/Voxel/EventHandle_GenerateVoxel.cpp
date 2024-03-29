// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Voxel/EventHandle_GenerateVoxel.h"

UEventHandle_GenerateVoxel::UEventHandle_GenerateVoxel()
{
	EventType = EEventType::Multicast;

	VoxelItem = FVoxelItem();
}

void UEventHandle_GenerateVoxel::Fill_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		VoxelItem = *InParams[0].GetPointerValue<FVoxelItem>();
	}
}

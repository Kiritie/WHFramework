// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Voxel/EventHandle_ChangeVoxelWorldState.h"

UEventHandle_ChangeVoxelWorldState::UEventHandle_ChangeVoxelWorldState()
{
	EventType = EEventType::Multicast;

	WorldState = EVoxelWorldState::None;
}

void UEventHandle_ChangeVoxelWorldState::Fill_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		WorldState = *InParams[0].GetPointerValue<EVoxelWorldState>();
	}
}

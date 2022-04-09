// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Voxel/EventHandle_ChangeVoxelWorldState.h"

UEventHandle_ChangeVoxelWorldState::UEventHandle_ChangeVoxelWorldState()
{
	EventType = EEventType::Multicast;

	WorldState = EVoxelWorldState::None;
}

void UEventHandle_ChangeVoxelWorldState::Fill_Implementation(const TArray<FParameter>& InParameters)
{
	if(InParameters.IsValidIndex(0))
	{
		WorldState = *InParameters[0].GetPointerValue<EVoxelWorldState>();
	}
}

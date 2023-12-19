// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Voxel/EventHandle_WorldStateChanged.h"

UEventHandle_WorldStateChanged::UEventHandle_WorldStateChanged()
{
	WorldState = EVoxelWorldState::None;
}

void UEventHandle_WorldStateChanged::Parse_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		WorldState = *InParams[0].GetPointerValue<EVoxelWorldState>();
	}
}

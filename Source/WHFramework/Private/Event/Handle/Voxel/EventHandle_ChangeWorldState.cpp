// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Voxel/EventHandle_ChangeWorldState.h"

UEventHandle_ChangeWorldState::UEventHandle_ChangeWorldState()
{
	WorldState = EVoxelWorldState::None;
}

void UEventHandle_ChangeWorldState::Parse_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		WorldState = *InParams[0].GetPointerValue<EVoxelWorldState>();
	}
}

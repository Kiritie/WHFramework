// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Voxel/EventHandle_WorldModeChanged.h"

UEventHandle_WorldModeChanged::UEventHandle_WorldModeChanged()
{
	WorldMode = EVoxelWorldMode::None;
}

void UEventHandle_WorldModeChanged::Parse_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		WorldMode = *InParams[0].GetPointerValue<EVoxelWorldMode>();
	}
}

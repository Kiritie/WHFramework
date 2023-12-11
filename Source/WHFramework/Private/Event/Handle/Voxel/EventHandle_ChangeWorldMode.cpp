// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Voxel/EventHandle_ChangeWorldMode.h"

UEventHandle_ChangeWorldMode::UEventHandle_ChangeWorldMode()
{
	WorldMode = EVoxelWorldMode::None;
}

void UEventHandle_ChangeWorldMode::Parse_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		WorldMode = *InParams[0].GetPointerValue<EVoxelWorldMode>();
	}
}

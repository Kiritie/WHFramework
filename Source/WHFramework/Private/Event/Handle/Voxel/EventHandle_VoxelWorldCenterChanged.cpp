// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Voxel/EventHandle_VoxelWorldCenterChanged.h"

UEventHandle_VoxelWorldCenterChanged::UEventHandle_VoxelWorldCenterChanged()
{
	WorldCenter = FIndex::ZeroIndex;
}

void UEventHandle_VoxelWorldCenterChanged::Parse_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		WorldCenter = InParams[0].GetPointerValueRef<FIndex>();
	}
}

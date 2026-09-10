// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Voxel/EventHandle_VoxelWorldAgentMoved.h"

UEventHandle_VoxelWorldAgentMoved::UEventHandle_VoxelWorldAgentMoved()
{
	AgentIndex = FIndex::ZeroIndex;
}

void UEventHandle_VoxelWorldAgentMoved::Parse_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		if(const FVoxelIndexParameterValue* Value = InParams[0].GetStructPtr<FVoxelIndexParameterValue>())
		{
			AgentIndex = Value->Value;
		}
	}
}

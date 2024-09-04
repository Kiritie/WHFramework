// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Scene/EventHandle_SetDataLayerRuntimeState.h"

#include "WorldPartition/DataLayer/DataLayerAsset.h"

UEventHandle_SetDataLayerRuntimeState::UEventHandle_SetDataLayerRuntimeState()
{
	DataLayer = nullptr;
	State = EDataLayerRuntimeState::Unloaded;
	bRecursive = false;
}

void UEventHandle_SetDataLayerRuntimeState::OnDespawn_Implementation(bool bRecovery)
{
	DataLayer = nullptr;
	State = EDataLayerRuntimeState::Unloaded;
	bRecursive = false;
}

void UEventHandle_SetDataLayerRuntimeState::Parse_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		DataLayer = InParams[0];
	}
	if(InParams.IsValidIndex(1))
	{
		State = InParams[1].GetPointerValueRef<EDataLayerRuntimeState>();
	}
	if(InParams.IsValidIndex(2))
	{
		bRecursive = InParams[2];
	}
}

TArray<FParameter> UEventHandle_SetDataLayerRuntimeState::Pack_Implementation()
{
	return { DataLayer, &State, bRecursive };
}

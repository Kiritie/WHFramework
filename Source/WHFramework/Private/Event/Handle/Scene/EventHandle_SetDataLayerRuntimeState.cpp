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
		DataLayer = InParams[0].Get<UDataLayerAsset*>();
	}
	if(InParams.IsValidIndex(1))
	{
		State = static_cast<EDataLayerRuntimeState>(InParams[1].Get<uint8>());
	}
	if(InParams.IsValidIndex(2))
	{
		bRecursive = InParams[2].Get<bool>();
	}
}

TArray<FParameter> UEventHandle_SetDataLayerRuntimeState::Pack_Implementation()
{
	return { DataLayer, static_cast<uint8>(State), bRecursive };
}

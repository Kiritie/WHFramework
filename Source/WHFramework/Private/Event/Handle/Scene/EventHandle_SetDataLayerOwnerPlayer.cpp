// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Scene/EventHandle_SetDataLayerOwnerPlayer.h"

#include "WorldPartition/DataLayer/DataLayerAsset.h"

UEventHandle_SetDataLayerOwnerPlayer::UEventHandle_SetDataLayerOwnerPlayer()
{
	DataLayer = nullptr;
	PlayerIndex = -1;
}

void UEventHandle_SetDataLayerOwnerPlayer::OnDespawn_Implementation(bool bRecovery)
{
	DataLayer = nullptr;
	PlayerIndex = -1;
}

void UEventHandle_SetDataLayerOwnerPlayer::Parse_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		DataLayer = InParams[0].GetObjectValue<UDataLayerAsset>();
	}
	if(InParams.IsValidIndex(1))
	{
		PlayerIndex = InParams[1].GetIntegerValue();
	}
}

TArray<FParameter> UEventHandle_SetDataLayerOwnerPlayer::Pack_Implementation()
{
	return { DataLayer, PlayerIndex };
}

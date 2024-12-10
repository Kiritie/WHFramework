// Fill out your copyright notice in the Description page of Project Settings.


#include "Event/Handle/Scene/EventHandle_SetLevelOwnerPlayer.h"

UEventHandle_SetLevelOwnerPlayer::UEventHandle_SetLevelOwnerPlayer()
{
	LevelObjectPtr = nullptr;
	LevelPath = NAME_None;
	PlayerIndex = -1;
}

void UEventHandle_SetLevelOwnerPlayer::OnDespawn_Implementation(bool bRecovery)
{
	LevelObjectPtr = nullptr;
	LevelPath = NAME_None;
	PlayerIndex = -1;
}

void UEventHandle_SetLevelOwnerPlayer::Parse_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		LevelObjectPtr = InParams[0];
	}
	if(InParams.IsValidIndex(1))
	{
		LevelPath = InParams[1];
	}
	if(InParams.IsValidIndex(2))
	{
		PlayerIndex = InParams[2];
	}
}

TArray<FParameter> UEventHandle_SetLevelOwnerPlayer::Pack_Implementation()
{
	return { LevelObjectPtr, LevelPath, PlayerIndex };
}

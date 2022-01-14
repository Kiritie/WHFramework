// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "SaveGame/SaveGameBase.h"

#include "Kismet/GameplayStatics.h"
#include "SaveGame/SaveGameDataBase.h"

USaveGameBase::USaveGameBase()
{
	// set default pawn class to our Blueprinted character

	SaveName = NAME_None;
	bSaved = false;
	bLoaded = false;
}

USaveGameDataBase* USaveGameBase::K2_GetSaveGameData(TSubclassOf<USaveGameDataBase> InSaveGameDataClass) const
{
	return SaveGameData;
}

void USaveGameBase::OnSave_Implementation()
{
	bSaved = true;
}

void USaveGameBase::OnCreate_Implementation(USaveGameDataBase* InSaveGameData)
{
	SaveGameData = InSaveGameData;
	if(SaveGameData)
	{
		SaveGameData->OnInitialize();
	}
}

void USaveGameBase::OnLoad_Implementation()
{
	bLoaded = true;
}

void USaveGameBase::OnUnload_Implementation()
{
	bLoaded = false;
}

void USaveGameBase::OnReset_Implementation()
{
}

void USaveGameBase::OnRefresh_Implementation()
{
}

void USaveGameBase::OnDestroy_Implementation()
{
	
}

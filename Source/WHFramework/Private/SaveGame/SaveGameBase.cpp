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

void USaveGameBase::OnCreate(USaveGameDataBase* InSaveGameData)
{
	SaveGameData = InSaveGameData;
	if(SaveGameData)
	{
		SaveGameData->OnInitialize();
	}
}

void USaveGameBase::OnSave()
{
	bSaved = true;
}

void USaveGameBase::OnLoad()
{
	bLoaded = true;
}

void USaveGameBase::OnUnload()
{
	bLoaded = false;
}

void USaveGameBase::OnReset()
{
}

void USaveGameBase::OnRefresh()
{
}

void USaveGameBase::OnDestroy()
{
	
}

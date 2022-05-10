// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "SaveGame/Base/SaveGameBase.h"
#include "Kismet/GameplayStatics.h"
#include "SaveGame/SaveGameModuleBPLibrary.h"

USaveGameBase::USaveGameBase()
{
	// set default pawn class to our Blueprinted character

	SaveName = NAME_None;
	SaveIndex = 0;
	bSaved = false;
	bLoaded = false;
}

void USaveGameBase::OnCreate_Implementation(int32 InSaveIndex)
{
	SaveIndex = InSaveIndex;
}

void USaveGameBase::OnSave_Implementation()
{
	bSaved = true;
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

bool USaveGameBase::Save(bool bRefresh)
{
	return USaveGameModuleBPLibrary::SaveSaveGame<USaveGameBase>(bRefresh, GetClass());
}

bool USaveGameBase::Load()
{
	return USaveGameModuleBPLibrary::LoadSaveGame<USaveGameBase>(SaveIndex, GetClass()) != nullptr;
}

bool USaveGameBase::Unload()
{
	return USaveGameModuleBPLibrary::UnloadSaveGame<USaveGameBase>(GetClass());
}

bool USaveGameBase::Reset()
{
	return USaveGameModuleBPLibrary::ResetSaveGame<USaveGameBase>(GetClass());
}

bool USaveGameBase::Refresh()
{
	return USaveGameModuleBPLibrary::RefreshSaveGame<USaveGameBase>(GetClass());
}

bool USaveGameBase::Destroy()
{
	return USaveGameModuleBPLibrary::DestroySaveGame<USaveGameBase>(SaveIndex, GetClass());
}

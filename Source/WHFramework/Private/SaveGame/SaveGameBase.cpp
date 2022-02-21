// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "SaveGame/SaveGameBase.h"

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

void USaveGameBase::OnCreate_Implementation()
{
	
}

void USaveGameBase::OnSave_Implementation(int32 InIndex)
{
	bSaved = true;
	SaveIndex = InIndex;
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

void USaveGameBase::Save(int32 InIndex, bool bRefresh)
{
	USaveGameModuleBPLibrary::SaveSaveGame<USaveGameBase>(InIndex, bRefresh, GetClass());
}

void USaveGameBase::Load()
{
	USaveGameModuleBPLibrary::LoadSaveGame<USaveGameBase>(SaveIndex, GetClass());
}

void USaveGameBase::Unload()
{
	USaveGameModuleBPLibrary::UnloadSaveGame<USaveGameBase>(GetClass());
}

void USaveGameBase::Reset()
{
	USaveGameModuleBPLibrary::ResetSaveGame<USaveGameBase>(GetClass());
}

void USaveGameBase::Refresh()
{
	USaveGameModuleBPLibrary::RefreshSaveGame<USaveGameBase>(GetClass());
}

void USaveGameBase::Destroy()
{
	USaveGameModuleBPLibrary::DestroySaveGame<USaveGameBase>(SaveIndex, GetClass());
}

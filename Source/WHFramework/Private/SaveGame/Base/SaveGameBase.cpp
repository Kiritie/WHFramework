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
}

void USaveGameBase::OnLoad_Implementation(bool bForceMode)
{
}

void USaveGameBase::OnUnload_Implementation(bool bForceMode)
{
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

void USaveGameBase::OnActiveChange_Implementation(bool bActive)
{
}

bool USaveGameBase::Save(bool bRefresh)
{
	return USaveGameModuleBPLibrary::SaveSaveGame(GetClass(), SaveIndex, bRefresh);
}

bool USaveGameBase::Load(bool bForceMode)
{
	return USaveGameModuleBPLibrary::LoadSaveGame(GetClass(), SaveIndex, bForceMode) != nullptr;
}

bool USaveGameBase::Unload(bool bForceMode)
{
	return USaveGameModuleBPLibrary::UnloadSaveGame(GetClass(), SaveIndex, bForceMode);
}

bool USaveGameBase::Reset()
{
	return USaveGameModuleBPLibrary::ResetSaveGame(GetClass(), SaveIndex);
}

bool USaveGameBase::Refresh()
{
	return USaveGameModuleBPLibrary::RefreshSaveGame(GetClass(), SaveIndex);
}

bool USaveGameBase::Destroy()
{
	return USaveGameModuleBPLibrary::DestroySaveGame(GetClass(), SaveIndex);
}

bool USaveGameBase::IsActived() const
{
	return USaveGameModuleBPLibrary::GetActiveSaveIndex(GetClass()) == SaveIndex;
}

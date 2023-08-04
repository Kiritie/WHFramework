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

void USaveGameBase::OnLoad_Implementation(EPhase InPhase)
{
}

void USaveGameBase::OnUnload_Implementation(EPhase InPhase)
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

bool USaveGameBase::Load(EPhase InPhase)
{
	return USaveGameModuleBPLibrary::LoadSaveGame(GetClass(), SaveIndex, InPhase) != nullptr;
}

bool USaveGameBase::Unload(EPhase InPhase)
{
	return USaveGameModuleBPLibrary::UnloadSaveGame(GetClass(), SaveIndex, InPhase);
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

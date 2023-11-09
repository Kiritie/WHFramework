// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "SaveGame/Base/SaveGameBase.h"
#include "SaveGame/SaveGameModuleStatics.h"

USaveGameBase::USaveGameBase()
{
	// set default pawn class to our Blueprinted character

	SaveName = NAME_None;
	SaveIndex = 0;
	bSaved = false;
	bLoaded = false;
}

void USaveGameBase::OnCreate_Implementation(int32 InIndex)
{
	SaveIndex = InIndex;
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

bool USaveGameBase::Save(bool bRefresh)
{
	return USaveGameModuleStatics::SaveSaveGame(GetClass(), SaveIndex, bRefresh);
}

bool USaveGameBase::Load(EPhase InPhase)
{
	return USaveGameModuleStatics::LoadSaveGame(GetClass(), SaveIndex, InPhase) != nullptr;
}

bool USaveGameBase::Unload(EPhase InPhase)
{
	return USaveGameModuleStatics::UnloadSaveGame(GetClass(), SaveIndex, InPhase);
}

bool USaveGameBase::Reset()
{
	return USaveGameModuleStatics::ResetSaveGame(GetClass(), SaveIndex);
}

bool USaveGameBase::Refresh()
{
	return USaveGameModuleStatics::RefreshSaveGame(GetClass(), SaveIndex);
}

bool USaveGameBase::Destroy()
{
	return USaveGameModuleStatics::DestroySaveGame(GetClass(), SaveIndex);
}

bool USaveGameBase::IsActived() const
{
	return USaveGameModuleStatics::GetSaveGameInfo(GetClass()).ActiveIndex == SaveIndex;
}

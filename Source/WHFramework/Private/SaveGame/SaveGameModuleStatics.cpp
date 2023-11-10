// Fill out your copyright notice in the Description page of Project Settings.


#include "SaveGame/SaveGameModuleStatics.h"
#include "SaveGame/SaveGameModule.h"
#include "SaveGame/Base/SaveDataInterface.h"

int32 USaveGameModuleStatics::GetUserIndex()
{
	return USaveGameModule::Get().GetUserIndex();
}

void USaveGameModuleStatics::SetUserIndex(int32 InUserIndex)
{
	USaveGameModule::Get().SetUserIndex(InUserIndex);
}

FString USaveGameModuleStatics::GetSaveSlotName(FName InSaveName, int32 InIndex)
{
	return USaveGameModule::Get().GetSlotName(InSaveName, InIndex);
}

bool USaveGameModuleStatics::HasSaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex)
{
	return USaveGameModule::Get().HasSaveGame(InClass, InIndex);
}

FSaveGameInfo USaveGameModuleStatics::GetSaveGameInfo(TSubclassOf<USaveGameBase> InClass)
{
	return USaveGameModule::Get().GetSaveGameInfo(InClass);
}

USaveGameBase* USaveGameModuleStatics::GetSaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex)
{
	return USaveGameModule::Get().GetSaveGame(InClass, InIndex);
}

TArray<USaveGameBase*> USaveGameModuleStatics::GetSaveGames(TSubclassOf<USaveGameBase> InClass)
{
	return USaveGameModule::Get().GetSaveGames(InClass);
}

USaveGameBase* USaveGameModuleStatics::CreateSaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex, EPhase InPhase)
{
	return USaveGameModule::Get().CreateSaveGame(InClass, InIndex, InPhase);
}

USaveGameBase* USaveGameModuleStatics::GetOrCreateSaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex, EPhase InPhase)
{
	return USaveGameModule::Get().GetOrCreateSaveGame(InClass, InIndex);
}

USaveGameBase* USaveGameModuleStatics::LoadOrCreateSaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex, EPhase InPhase)
{
	return USaveGameModule::Get().LoadOrCreateSaveGame(InClass, InIndex, InPhase);
}

bool USaveGameModuleStatics::SaveSaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex, bool bRefresh)
{
	return USaveGameModule::Get().SaveSaveGame(InClass, InIndex, bRefresh);
}

bool USaveGameModuleStatics::SaveSaveGames(TSubclassOf<USaveGameBase> InClass, bool bRefresh)
{
	return USaveGameModule::Get().SaveSaveGames(InClass, bRefresh);
}

bool USaveGameModuleStatics::SaveAllSaveGame(bool bRefresh)
{
	return USaveGameModule::Get().SaveAllSaveGame(bRefresh);
}

USaveGameBase* USaveGameModuleStatics::LoadSaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex, EPhase InPhase)
{
	return USaveGameModule::Get().LoadSaveGame(InClass, InIndex, InPhase);
}

bool USaveGameModuleStatics::UnloadSaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex, EPhase InPhase)
{
	return USaveGameModule::Get().UnloadSaveGame(InClass, InIndex, InPhase);
}

bool USaveGameModuleStatics::ResetSaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex)
{
	return USaveGameModule::Get().ResetSaveGame(InClass, InIndex);
}

bool USaveGameModuleStatics::RefreshSaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex)
{
	return USaveGameModule::Get().RefreshSaveGame(InClass, InIndex);
}

bool USaveGameModuleStatics::DestroySaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex)
{
	return USaveGameModule::Get().DestroySaveGame(InClass, InIndex);
}

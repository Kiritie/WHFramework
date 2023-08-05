// Fill out your copyright notice in the Description page of Project Settings.


#include "SaveGame/SaveGameModuleBPLibrary.h"
#include "Global/GlobalBPLibrary.h"
#include "SaveGame/SaveGameModule.h"
#include "SaveGame/Base/SaveDataInterface.h"

int32 USaveGameModuleBPLibrary::GetUserIndex()
{
	if(ASaveGameModule* SaveGameModule = ASaveGameModule::Get())
	{
		return SaveGameModule->GetUserIndex();
	}
	return -1;
}

void USaveGameModuleBPLibrary::SetUserIndex(int32 InUserIndex)
{
	if(ASaveGameModule* SaveGameModule = ASaveGameModule::Get())
	{
		SaveGameModule->SetUserIndex(InUserIndex);
	}
}

FString USaveGameModuleBPLibrary::GetSaveSlotName(FName InSaveName, int32 InSaveIndex)
{
	if(ASaveGameModule* SaveGameModule = ASaveGameModule::Get())
	{
		return SaveGameModule->GetSaveSlotName(InSaveName, InSaveIndex);
	}
	return TEXT("");
}

bool USaveGameModuleBPLibrary::HasSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex, bool bFindOnDisk, bool bNeedLoaded)
{
	if(ASaveGameModule* SaveGameModule = ASaveGameModule::Get())
	{
		return SaveGameModule->HasSaveGame(InSaveGameClass, InSaveIndex, bFindOnDisk, bNeedLoaded);
	}
	return false;
}

int32 USaveGameModuleBPLibrary::GetValidSaveIndex(TSubclassOf<USaveGameBase> InSaveGameClass)
{
	if(ASaveGameModule* SaveGameModule = ASaveGameModule::Get())
	{
		return SaveGameModule->GetValidSaveIndex(InSaveGameClass);
	}
	return -1;
}

int32 USaveGameModuleBPLibrary::GetActiveSaveIndex(TSubclassOf<USaveGameBase> InSaveGameClass)
{
	if(ASaveGameModule* SaveGameModule = ASaveGameModule::Get())
	{
		return SaveGameModule->GetActiveSaveIndex(InSaveGameClass);
	}
	return -1;
}

void USaveGameModuleBPLibrary::SetActiveSaveIndex(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex)
{
	if(ASaveGameModule* SaveGameModule = ASaveGameModule::Get())
	{
		SaveGameModule->SetActiveSaveIndex(InSaveGameClass, InSaveIndex);
	}
}

FSaveGameInfo USaveGameModuleBPLibrary::GetSaveGameInfo(TSubclassOf<USaveGameBase> InSaveGameClass)
{
	if(ASaveGameModule* SaveGameModule = ASaveGameModule::Get())
	{
		return SaveGameModule->GetSaveGameInfo(InSaveGameClass);
	}
	return FSaveGameInfo();
}

USaveGameBase* USaveGameModuleBPLibrary::GetSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex)
{
	if(ASaveGameModule* SaveGameModule = ASaveGameModule::Get())
	{
		return SaveGameModule->GetSaveGame(InSaveGameClass, InSaveIndex);
	}
	return nullptr;
}

TArray<USaveGameBase*> USaveGameModuleBPLibrary::GetSaveGames(TSubclassOf<USaveGameBase> InSaveGameClass)
{
	if(ASaveGameModule* SaveGameModule = ASaveGameModule::Get())
	{
		return SaveGameModule->GetSaveGames(InSaveGameClass);
	}
	return TArray<USaveGameBase*>();
}

USaveGameBase* USaveGameModuleBPLibrary::CreateSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex, bool bAutoLoad)
{
	if(ASaveGameModule* SaveGameModule = ASaveGameModule::Get())
	{
		return SaveGameModule->CreateSaveGame(InSaveGameClass, InSaveIndex, bAutoLoad);
	}
	return nullptr;
}

USaveGameBase* USaveGameModuleBPLibrary::GetOrCreateSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex, bool bAutoLoad)
{
	if(ASaveGameModule* SaveGameModule = ASaveGameModule::Get())
	{
		return SaveGameModule->GetOrCreateSaveGame(InSaveGameClass, InSaveIndex, bAutoLoad);
	}
	return nullptr;
}

USaveGameBase* USaveGameModuleBPLibrary::LoadOrCreateSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex)
{
	if(ASaveGameModule* SaveGameModule = ASaveGameModule::Get())
	{
		return SaveGameModule->LoadOrCreateSaveGame(InSaveGameClass, InSaveIndex);
	}
	return nullptr;
}

bool USaveGameModuleBPLibrary::SaveSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex, bool bRefresh)
{
	if(ASaveGameModule* SaveGameModule = ASaveGameModule::Get())
	{
		return SaveGameModule->SaveSaveGame(InSaveGameClass, InSaveIndex, bRefresh);
	}
	return false;
}

bool USaveGameModuleBPLibrary::SaveSaveGames(TSubclassOf<USaveGameBase> InSaveGameClass, bool bRefresh)
{
	if(ASaveGameModule* SaveGameModule = ASaveGameModule::Get())
	{
		return SaveGameModule->SaveSaveGames(InSaveGameClass, bRefresh);
	}
	return false;
}

bool USaveGameModuleBPLibrary::SaveAllSaveGame(bool bRefresh)
{
	if(ASaveGameModule* SaveGameModule = ASaveGameModule::Get())
	{
		return SaveGameModule->SaveAllSaveGame(bRefresh);
	}
	return false;
}

USaveGameBase* USaveGameModuleBPLibrary::LoadSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex, EPhase InPhase)
{
	if(ASaveGameModule* SaveGameModule = ASaveGameModule::Get())
	{
		return SaveGameModule->LoadSaveGame(InSaveGameClass, InSaveIndex, InPhase);
	}
	return nullptr;
}

bool USaveGameModuleBPLibrary::UnloadSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex, EPhase InPhase)
{
	if(ASaveGameModule* SaveGameModule = ASaveGameModule::Get())
	{
		return SaveGameModule->UnloadSaveGame(InSaveGameClass, InSaveIndex, InPhase);
	}
	return false;
}

bool USaveGameModuleBPLibrary::ResetSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex)
{
	if(ASaveGameModule* SaveGameModule = ASaveGameModule::Get())
	{
		return SaveGameModule->ResetSaveGame(InSaveGameClass, InSaveIndex);
	}
	return false;
}

bool USaveGameModuleBPLibrary::RefreshSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex)
{
	if(ASaveGameModule* SaveGameModule = ASaveGameModule::Get())
	{
		return SaveGameModule->RefreshSaveGame(InSaveGameClass, InSaveIndex);
	}
	return false;
}

bool USaveGameModuleBPLibrary::DestroySaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex)
{
	if(ASaveGameModule* SaveGameModule = ASaveGameModule::Get())
	{
		return SaveGameModule->DestroySaveGame(InSaveGameClass, InSaveIndex);
	}
	return false;
}

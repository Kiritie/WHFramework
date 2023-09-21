// Fill out your copyright notice in the Description page of Project Settings.


#include "SaveGame/SaveGameModuleBPLibrary.h"
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

FString USaveGameModuleBPLibrary::GetSaveSlotName(FName InSaveName, int32 InIndex)
{
	if(ASaveGameModule* SaveGameModule = ASaveGameModule::Get())
	{
		return SaveGameModule->GetSaveSlotName(InSaveName, InIndex);
	}
	return TEXT("");
}

bool USaveGameModuleBPLibrary::HasSaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex)
{
	if(ASaveGameModule* SaveGameModule = ASaveGameModule::Get())
	{
		return SaveGameModule->HasSaveGame(InClass, InIndex);
	}
	return false;
}

FSaveGameInfo USaveGameModuleBPLibrary::GetSaveGameInfo(TSubclassOf<USaveGameBase> InClass)
{
	if(ASaveGameModule* SaveGameModule = ASaveGameModule::Get())
	{
		return SaveGameModule->GetSaveGameInfo(InClass);
	}
	return FSaveGameInfo();
}

USaveGameBase* USaveGameModuleBPLibrary::GetSaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex)
{
	if(ASaveGameModule* SaveGameModule = ASaveGameModule::Get())
	{
		return SaveGameModule->GetSaveGame(InClass, InIndex);
	}
	return nullptr;
}

TArray<USaveGameBase*> USaveGameModuleBPLibrary::GetSaveGames(TSubclassOf<USaveGameBase> InClass)
{
	if(ASaveGameModule* SaveGameModule = ASaveGameModule::Get())
	{
		return SaveGameModule->GetSaveGames(InClass);
	}
	return TArray<USaveGameBase*>();
}

USaveGameBase* USaveGameModuleBPLibrary::CreateSaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex, EPhase InPhase)
{
	if(ASaveGameModule* SaveGameModule = ASaveGameModule::Get())
	{
		return SaveGameModule->CreateSaveGame(InClass, InIndex, InPhase);
	}
	return nullptr;
}

USaveGameBase* USaveGameModuleBPLibrary::GetOrCreateSaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex, EPhase InPhase)
{
	if(ASaveGameModule* SaveGameModule = ASaveGameModule::Get())
	{
		return SaveGameModule->GetOrCreateSaveGame(InClass, InIndex);
	}
	return nullptr;
}

USaveGameBase* USaveGameModuleBPLibrary::LoadOrCreateSaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex, EPhase InPhase)
{
	if(ASaveGameModule* SaveGameModule = ASaveGameModule::Get())
	{
		return SaveGameModule->LoadOrCreateSaveGame(InClass, InIndex, InPhase);
	}
	return nullptr;
}

bool USaveGameModuleBPLibrary::SaveSaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex, bool bRefresh)
{
	if(ASaveGameModule* SaveGameModule = ASaveGameModule::Get())
	{
		return SaveGameModule->SaveSaveGame(InClass, InIndex, bRefresh);
	}
	return false;
}

bool USaveGameModuleBPLibrary::SaveSaveGames(TSubclassOf<USaveGameBase> InClass, bool bRefresh)
{
	if(ASaveGameModule* SaveGameModule = ASaveGameModule::Get())
	{
		return SaveGameModule->SaveSaveGames(InClass, bRefresh);
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

USaveGameBase* USaveGameModuleBPLibrary::LoadSaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex, EPhase InPhase)
{
	if(ASaveGameModule* SaveGameModule = ASaveGameModule::Get())
	{
		return SaveGameModule->LoadSaveGame(InClass, InIndex, InPhase);
	}
	return nullptr;
}

bool USaveGameModuleBPLibrary::UnloadSaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex, EPhase InPhase)
{
	if(ASaveGameModule* SaveGameModule = ASaveGameModule::Get())
	{
		return SaveGameModule->UnloadSaveGame(InClass, InIndex, InPhase);
	}
	return false;
}

bool USaveGameModuleBPLibrary::ResetSaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex)
{
	if(ASaveGameModule* SaveGameModule = ASaveGameModule::Get())
	{
		return SaveGameModule->ResetSaveGame(InClass, InIndex);
	}
	return false;
}

bool USaveGameModuleBPLibrary::RefreshSaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex)
{
	if(ASaveGameModule* SaveGameModule = ASaveGameModule::Get())
	{
		return SaveGameModule->RefreshSaveGame(InClass, InIndex);
	}
	return false;
}

bool USaveGameModuleBPLibrary::DestroySaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex)
{
	if(ASaveGameModule* SaveGameModule = ASaveGameModule::Get())
	{
		return SaveGameModule->DestroySaveGame(InClass, InIndex);
	}
	return false;
}

// Fill out your copyright notice in the Description page of Project Settings.


#include "SaveGame/SaveGameModuleBPLibrary.h"
#include "Global/GlobalBPLibrary.h"
#include "SaveGame/SaveGameModule.h"
#include "SaveGame/Base/SaveDataInterface.h"

int32 USaveGameModuleBPLibrary::GetUserIndex()
{
	if(ASaveGameModule* SaveGameModule = AMainModule::GetModuleByClass<ASaveGameModule>())
	{
		return SaveGameModule->GetUserIndex();
	}
	return -1;
}

void USaveGameModuleBPLibrary::SetUserIndex(int32 InUserIndex)
{
	if(ASaveGameModule* SaveGameModule = AMainModule::GetModuleByClass<ASaveGameModule>())
	{
		SaveGameModule->SetUserIndex(InUserIndex);
	}
}

FString USaveGameModuleBPLibrary::GetSaveSlotName(FName InSaveName, int32 InSaveIndex)
{
	if(ASaveGameModule* SaveGameModule = AMainModule::GetModuleByClass<ASaveGameModule>())
	{
		return SaveGameModule->GetSaveSlotName(InSaveName, InSaveIndex);
	}
	return TEXT("");
}

bool USaveGameModuleBPLibrary::HasSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex, bool bFindOnDisk, bool bNeedLoaded)
{
	if(ASaveGameModule* SaveGameModule = AMainModule::GetModuleByClass<ASaveGameModule>())
	{
		return SaveGameModule->HasSaveGame(InSaveGameClass, InSaveIndex, bFindOnDisk, bNeedLoaded);
	}
	return false;
}

int32 USaveGameModuleBPLibrary::GetValidSaveIndex(TSubclassOf<USaveGameBase> InSaveGameClass)
{
	if(ASaveGameModule* SaveGameModule = AMainModule::GetModuleByClass<ASaveGameModule>())
	{
		return SaveGameModule->GetValidSaveIndex(InSaveGameClass);
	}
	return -1;
}

int32 USaveGameModuleBPLibrary::GetActiveSaveIndex(TSubclassOf<USaveGameBase> InSaveGameClass)
{
	if(ASaveGameModule* SaveGameModule = AMainModule::GetModuleByClass<ASaveGameModule>())
	{
		return SaveGameModule->GetActiveSaveIndex(InSaveGameClass);
	}
	return -1;
}

void USaveGameModuleBPLibrary::SetActiveSaveIndex(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex)
{
	if(ASaveGameModule* SaveGameModule = AMainModule::GetModuleByClass<ASaveGameModule>())
	{
		SaveGameModule->SetActiveSaveIndex(InSaveGameClass, InSaveIndex);
	}
}

USaveGameBase* USaveGameModuleBPLibrary::GetSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex)
{
	if(ASaveGameModule* SaveGameModule = AMainModule::GetModuleByClass<ASaveGameModule>())
	{
		return SaveGameModule->GetSaveGame(InSaveGameClass, InSaveIndex);
	}
	return nullptr;
}

TArray<USaveGameBase*> USaveGameModuleBPLibrary::GetSaveGames(TSubclassOf<USaveGameBase> InSaveGameClass)
{
	if(ASaveGameModule* SaveGameModule = AMainModule::GetModuleByClass<ASaveGameModule>())
	{
		return SaveGameModule->GetSaveGames(InSaveGameClass);
	}
	return TArray<USaveGameBase*>();
}

USaveGameBase* USaveGameModuleBPLibrary::CreateSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex, bool bAutoLoad)
{
	if(ASaveGameModule* SaveGameModule = AMainModule::GetModuleByClass<ASaveGameModule>())
	{
		return SaveGameModule->CreateSaveGame(InSaveGameClass, InSaveIndex, bAutoLoad);
	}
	return nullptr;
}

USaveGameBase* USaveGameModuleBPLibrary::GetOrCreateSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex, bool bAutoLoad)
{
	if(ASaveGameModule* SaveGameModule = AMainModule::GetModuleByClass<ASaveGameModule>())
	{
		return SaveGameModule->GetOrCreateSaveGame(InSaveGameClass, InSaveIndex, bAutoLoad);
	}
	return nullptr;
}

USaveGameBase* USaveGameModuleBPLibrary::LoadOrCreateSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex)
{
	if(ASaveGameModule* SaveGameModule = AMainModule::GetModuleByClass<ASaveGameModule>())
	{
		return SaveGameModule->LoadOrCreateSaveGame(InSaveGameClass, InSaveIndex);
	}
	return nullptr;
}

bool USaveGameModuleBPLibrary::SaveSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex, bool bRefresh)
{
	if(ASaveGameModule* SaveGameModule = AMainModule::GetModuleByClass<ASaveGameModule>())
	{
		return SaveGameModule->SaveSaveGame(InSaveGameClass, InSaveIndex, bRefresh);
	}
	return false;
}

bool USaveGameModuleBPLibrary::SaveSaveGames(TSubclassOf<USaveGameBase> InSaveGameClass, bool bRefresh)
{
	if(ASaveGameModule* SaveGameModule = AMainModule::GetModuleByClass<ASaveGameModule>())
	{
		return SaveGameModule->SaveSaveGames(InSaveGameClass, bRefresh);
	}
	return false;
}

bool USaveGameModuleBPLibrary::SaveAllSaveGame(bool bRefresh)
{
	if(ASaveGameModule* SaveGameModule = AMainModule::GetModuleByClass<ASaveGameModule>())
	{
		return SaveGameModule->SaveAllSaveGame(bRefresh);
	}
	return false;
}

USaveGameBase* USaveGameModuleBPLibrary::LoadSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex)
{
	if(ASaveGameModule* SaveGameModule = AMainModule::GetModuleByClass<ASaveGameModule>())
	{
		return SaveGameModule->LoadSaveGame(InSaveGameClass, InSaveIndex);
	}
	return nullptr;
}

bool USaveGameModuleBPLibrary::UnloadSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex, bool bForceMode)
{
	if(ASaveGameModule* SaveGameModule = AMainModule::GetModuleByClass<ASaveGameModule>())
	{
		return SaveGameModule->UnloadSaveGame(InSaveGameClass, InSaveIndex, bForceMode);
	}
	return false;
}

bool USaveGameModuleBPLibrary::ResetSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex)
{
	if(ASaveGameModule* SaveGameModule = AMainModule::GetModuleByClass<ASaveGameModule>())
	{
		return SaveGameModule->ResetSaveGame(InSaveGameClass, InSaveIndex);
	}
	return false;
}

bool USaveGameModuleBPLibrary::RefreshSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex)
{
	if(ASaveGameModule* SaveGameModule = AMainModule::GetModuleByClass<ASaveGameModule>())
	{
		return SaveGameModule->RefreshSaveGame(InSaveGameClass, InSaveIndex);
	}
	return false;
}

bool USaveGameModuleBPLibrary::DestroySaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex)
{
	if(ASaveGameModule* SaveGameModule = AMainModule::GetModuleByClass<ASaveGameModule>())
	{
		return SaveGameModule->DestroySaveGame(InSaveGameClass, InSaveIndex);
	}
	return false;
}

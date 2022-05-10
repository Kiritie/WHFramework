// Fill out your copyright notice in the Description page of Project Settings.


#include "SaveGame/SaveGameModuleBPLibrary.h"

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

bool USaveGameModuleBPLibrary::K2_HasSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex)
{
	if(ASaveGameModule* SaveGameModule = AMainModule::GetModuleByClass<ASaveGameModule>())
	{
		return SaveGameModule->K2_HasSaveGame(InSaveGameClass, InSaveIndex);
	}
	return false;
}

USaveGameBase* USaveGameModuleBPLibrary::K2_GetSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass)
{
	if(ASaveGameModule* SaveGameModule = AMainModule::GetModuleByClass<ASaveGameModule>())
	{
		return SaveGameModule->K2_GetSaveGame(InSaveGameClass);
	}
	return nullptr;
}

USaveGameBase* USaveGameModuleBPLibrary::K2_CreateSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex, bool bAutoLoad)
{
	if(ASaveGameModule* SaveGameModule = AMainModule::GetModuleByClass<ASaveGameModule>())
	{
		return SaveGameModule->K2_CreateSaveGame(InSaveGameClass, InSaveIndex, bAutoLoad);
	}
	return nullptr;
}

bool USaveGameModuleBPLibrary::K2_SaveSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, bool bRefresh)
{
	if(ASaveGameModule* SaveGameModule = AMainModule::GetModuleByClass<ASaveGameModule>())
	{
		return SaveGameModule->K2_SaveSaveGame(InSaveGameClass, bRefresh);
	}
	return false;
}

bool USaveGameModuleBPLibrary::SaveSaveGames(TArray<TSubclassOf<USaveGameBase>> InSaveGameClass, bool bRefresh)
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

USaveGameBase* USaveGameModuleBPLibrary::K2_LoadSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex)
{
	if(ASaveGameModule* SaveGameModule = AMainModule::GetModuleByClass<ASaveGameModule>())
	{
		return SaveGameModule->K2_LoadSaveGame(InSaveGameClass, InSaveIndex);
	}
	return nullptr;
}

bool USaveGameModuleBPLibrary::K2_UnloadSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass)
{
	if(ASaveGameModule* SaveGameModule = AMainModule::GetModuleByClass<ASaveGameModule>())
	{
		return SaveGameModule->K2_UnloadSaveGame(InSaveGameClass);
	}
	return false;
}

bool USaveGameModuleBPLibrary::K2_ResetSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass)
{
	if(ASaveGameModule* SaveGameModule = AMainModule::GetModuleByClass<ASaveGameModule>())
	{
		return SaveGameModule->K2_ResetSaveGame(InSaveGameClass);
	}
	return false;
}

bool USaveGameModuleBPLibrary::K2_RefreshSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass)
{
	if(ASaveGameModule* SaveGameModule = AMainModule::GetModuleByClass<ASaveGameModule>())
	{
		return SaveGameModule->K2_RefreshSaveGame(InSaveGameClass);
	}
	return false;
}

bool USaveGameModuleBPLibrary::K2_DestroySaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex)
{
	if(ASaveGameModule* SaveGameModule = AMainModule::GetModuleByClass<ASaveGameModule>())
	{
		return SaveGameModule->K2_DestroySaveGame(InSaveGameClass, InSaveIndex);
	}
	return false;
}

// Fill out your copyright notice in the Description page of Project Settings.


#include "SaveGame/SaveGameModuleStatics.h"
#include "SaveGame/SaveGameModule.h"
#include "SaveGame/Base/SaveDataInterface.h"

int32 USaveGameModuleStatics::GetUserIndex()
{
	if(USaveGameModule* SaveGameModule = USaveGameModule::Get())
	{
		return SaveGameModule->GetUserIndex();
	}
	return -1;
}

void USaveGameModuleStatics::SetUserIndex(int32 InUserIndex)
{
	if(USaveGameModule* SaveGameModule = USaveGameModule::Get())
	{
		SaveGameModule->SetUserIndex(InUserIndex);
	}
}

FString USaveGameModuleStatics::GetSaveSlotName(FName InSaveName, int32 InIndex)
{
	if(USaveGameModule* SaveGameModule = USaveGameModule::Get())
	{
		return SaveGameModule->GetSlotName(InSaveName, InIndex);
	}
	return TEXT("");
}

bool USaveGameModuleStatics::HasSaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex)
{
	if(USaveGameModule* SaveGameModule = USaveGameModule::Get())
	{
		return SaveGameModule->HasSaveGame(InClass, InIndex);
	}
	return false;
}

FSaveGameInfo USaveGameModuleStatics::GetSaveGameInfo(TSubclassOf<USaveGameBase> InClass)
{
	if(USaveGameModule* SaveGameModule = USaveGameModule::Get())
	{
		return SaveGameModule->GetSaveGameInfo(InClass);
	}
	return FSaveGameInfo();
}

USaveGameBase* USaveGameModuleStatics::GetSaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex)
{
	if(USaveGameModule* SaveGameModule = USaveGameModule::Get())
	{
		return SaveGameModule->GetSaveGame(InClass, InIndex);
	}
	return nullptr;
}

TArray<USaveGameBase*> USaveGameModuleStatics::GetSaveGames(TSubclassOf<USaveGameBase> InClass)
{
	if(USaveGameModule* SaveGameModule = USaveGameModule::Get())
	{
		return SaveGameModule->GetSaveGames(InClass);
	}
	return TArray<USaveGameBase*>();
}

USaveGameBase* USaveGameModuleStatics::CreateSaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex, EPhase InPhase)
{
	if(USaveGameModule* SaveGameModule = USaveGameModule::Get())
	{
		return SaveGameModule->CreateSaveGame(InClass, InIndex, InPhase);
	}
	return nullptr;
}

USaveGameBase* USaveGameModuleStatics::GetOrCreateSaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex, EPhase InPhase)
{
	if(USaveGameModule* SaveGameModule = USaveGameModule::Get())
	{
		return SaveGameModule->GetOrCreateSaveGame(InClass, InIndex);
	}
	return nullptr;
}

USaveGameBase* USaveGameModuleStatics::LoadOrCreateSaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex, EPhase InPhase)
{
	if(USaveGameModule* SaveGameModule = USaveGameModule::Get())
	{
		return SaveGameModule->LoadOrCreateSaveGame(InClass, InIndex, InPhase);
	}
	return nullptr;
}

bool USaveGameModuleStatics::SaveSaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex, bool bRefresh)
{
	if(USaveGameModule* SaveGameModule = USaveGameModule::Get())
	{
		return SaveGameModule->SaveSaveGame(InClass, InIndex, bRefresh);
	}
	return false;
}

bool USaveGameModuleStatics::SaveSaveGames(TSubclassOf<USaveGameBase> InClass, bool bRefresh)
{
	if(USaveGameModule* SaveGameModule = USaveGameModule::Get())
	{
		return SaveGameModule->SaveSaveGames(InClass, bRefresh);
	}
	return false;
}

bool USaveGameModuleStatics::SaveAllSaveGame(bool bRefresh)
{
	if(USaveGameModule* SaveGameModule = USaveGameModule::Get())
	{
		return SaveGameModule->SaveAllSaveGame(bRefresh);
	}
	return false;
}

USaveGameBase* USaveGameModuleStatics::LoadSaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex, EPhase InPhase)
{
	if(USaveGameModule* SaveGameModule = USaveGameModule::Get())
	{
		return SaveGameModule->LoadSaveGame(InClass, InIndex, InPhase);
	}
	return nullptr;
}

bool USaveGameModuleStatics::UnloadSaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex, EPhase InPhase)
{
	if(USaveGameModule* SaveGameModule = USaveGameModule::Get())
	{
		return SaveGameModule->UnloadSaveGame(InClass, InIndex, InPhase);
	}
	return false;
}

bool USaveGameModuleStatics::ResetSaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex)
{
	if(USaveGameModule* SaveGameModule = USaveGameModule::Get())
	{
		return SaveGameModule->ResetSaveGame(InClass, InIndex);
	}
	return false;
}

bool USaveGameModuleStatics::RefreshSaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex)
{
	if(USaveGameModule* SaveGameModule = USaveGameModule::Get())
	{
		return SaveGameModule->RefreshSaveGame(InClass, InIndex);
	}
	return false;
}

bool USaveGameModuleStatics::DestroySaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex)
{
	if(USaveGameModule* SaveGameModule = USaveGameModule::Get())
	{
		return SaveGameModule->DestroySaveGame(InClass, InIndex);
	}
	return false;
}

// Fill out your copyright notice in the Description page of Project Settings.


#include "SaveGame/SaveGameModule.h"

#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "SaveGame/Base/SaveGameBase.h"

// ParamSets default values
ASaveGameModule::ASaveGameModule()
{
	ModuleName = FName("SaveGameModule");

	UserIndex = 0;
}

#if WITH_EDITOR
void ASaveGameModule::OnGenerate_Implementation()
{
	Super::OnGenerate_Implementation();
}

void ASaveGameModule::OnDestroy_Implementation()
{
	Super::OnDestroy_Implementation();
}
#endif

void ASaveGameModule::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();
}

void ASaveGameModule::OnPreparatory_Implementation()
{
	Super::OnPreparatory_Implementation();
}

void ASaveGameModule::OnRefresh_Implementation(float DeltaSeconds)
{
	Super::OnRefresh_Implementation(DeltaSeconds);
}

void ASaveGameModule::OnPause_Implementation()
{
	Super::OnPause_Implementation();
}

void ASaveGameModule::OnUnPause_Implementation()
{
	Super::OnUnPause_Implementation();
}

FString ASaveGameModule::GetSaveSlotName(FName InSaveName, int32 InSaveIndex) const
{
	return InSaveIndex > 0 ? FString::Printf(TEXT("SaveGame_%s%d"), *InSaveName.ToString(), InSaveIndex) : FString::Printf(TEXT("SaveGame_%s"), *InSaveName.ToString());
}

bool ASaveGameModule::HasSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex, bool bFindOnDisk) const
{
	if(!InSaveGameClass) return false;

	const FName SaveName = InSaveGameClass.GetDefaultObject()->GetSaveName();
	if(!bFindOnDisk)
	{
		return AllSaveGames.Contains(SaveName) && AllSaveGames[SaveName].Array.IsValidIndex(InSaveIndex);
	}
	else
	{
		return UGameplayStatics::DoesSaveGameExist(GetSaveSlotName(SaveName, InSaveIndex), UserIndex);
	}
}

USaveGameBase* ASaveGameModule::GetSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex) const
{
	if(!InSaveGameClass) return nullptr;

	const FName SaveName = InSaveGameClass.GetDefaultObject()->GetSaveName();
	if(HasSaveGame(InSaveGameClass, InSaveIndex))
	{
		return AllSaveGames[SaveName].Array[InSaveIndex];
	}
	return nullptr;
}

TArray<USaveGameBase*> ASaveGameModule::GetSaveGames(TSubclassOf<USaveGameBase> InSaveGameClass) const
{
	if(!InSaveGameClass) return TArray<USaveGameBase*>();

	const FName SaveName = InSaveGameClass.GetDefaultObject()->GetSaveName();
	if(AllSaveGames.Contains(SaveName))
	{
		return AllSaveGames[SaveName].Array;
	}
	return TArray<USaveGameBase*>();
}

USaveGameBase* ASaveGameModule::CreateSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex, bool bAutoLoad)
{
	if(!InSaveGameClass) return nullptr;
		
	const FName SaveName = InSaveGameClass.GetDefaultObject()->GetSaveName();
	if(!HasSaveGame(InSaveGameClass, InSaveIndex))
	{
		if(USaveGameBase* SaveGame = Cast<USaveGameBase>(UGameplayStatics::CreateSaveGameObject(InSaveGameClass)))
		{
			if(!AllSaveGames.Contains(SaveName)) AllSaveGames.Add(SaveName);
			AllSaveGames[SaveName].Array.EmplaceAt(InSaveIndex, SaveGame);
			SaveGame->OnCreate(InSaveIndex);
			if(bAutoLoad)
			{
				SaveGame->Load();
			}
			return SaveGame;
		}
	}
	return nullptr;
}

USaveGameBase* ASaveGameModule::GetOrCreateSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex, bool bAutoLoad)
{
	return HasSaveGame(InSaveGameClass, InSaveIndex) ? GetSaveGame(InSaveGameClass, InSaveIndex) : CreateSaveGame(InSaveGameClass, InSaveIndex, bAutoLoad);
}

bool ASaveGameModule::SaveSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex, bool bRefresh)
{
	if(!InSaveGameClass) return false;

	const FName SaveName = InSaveGameClass.GetDefaultObject()->GetSaveName();
	if(HasSaveGame(InSaveGameClass, InSaveIndex))
	{
		if(USaveGameBase* SaveGame = GetSaveGame(InSaveGameClass, InSaveIndex))
		{
			if(bRefresh)
			{
				SaveGame->OnRefresh();
			}
			SaveGame->OnSave();
			return UGameplayStatics::SaveGameToSlot(SaveGame, GetSaveSlotName(SaveName, SaveGame->GetSaveIndex()), UserIndex);
		}
		return true;
	}
	return false;
}

bool ASaveGameModule::SaveSaveGames(TSubclassOf<USaveGameBase> InSaveGameClass, bool bRefresh)
{
	bool bIsAllSaved = true;
	const FName SaveName = InSaveGameClass.GetDefaultObject()->GetSaveName();
	if(AllSaveGames.Contains(SaveName))
	{
		for(int32 i = 0; i < AllSaveGames[SaveName].Array.Num(); i++)
		{
			if(!SaveSaveGame<USaveGameBase>(i, bRefresh, InSaveGameClass))
			{
				bIsAllSaved = false;
			}
		}
	}
	return bIsAllSaved;
}

bool ASaveGameModule::SaveAllSaveGame(bool bRefresh)
{
	bool bIsAllSaved = true;
	for(auto Iter1 : AllSaveGames)
	{
		for(auto Iter2 : Iter1.Value.Array)
		{
			if(!Iter2->Save(bRefresh))
			{
				bIsAllSaved = false;
			}
		}
	}
	return bIsAllSaved;
}

USaveGameBase* ASaveGameModule::LoadSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex)
{
	if(!InSaveGameClass) return nullptr;

	const FName SaveName = InSaveGameClass.GetDefaultObject()->GetSaveName();
	if (HasSaveGame(InSaveGameClass, InSaveIndex))
	{
		if(USaveGameBase* SaveGame = GetSaveGame(InSaveGameClass, InSaveIndex))
		{
			SaveGame->OnLoad();
			return SaveGame;
		}
	}
	else if(HasSaveGame(InSaveGameClass, InSaveIndex, true))
	{
		if(USaveGameBase* SaveGame = Cast<USaveGameBase>(UGameplayStatics::LoadGameFromSlot(GetSaveSlotName(SaveName, InSaveIndex), UserIndex)))
		{
			if(!AllSaveGames.Contains(SaveName)) AllSaveGames.Add(SaveName);
			AllSaveGames[SaveName].Array.EmplaceAt(InSaveIndex, SaveGame);
			SaveGame->OnLoad();
			return SaveGame;
		}
	}
	return nullptr;
}

bool ASaveGameModule::UnloadSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex)
{
	if(!InSaveGameClass) return false;

	const FName SaveName = InSaveGameClass.GetDefaultObject()->GetSaveName();
	if(HasSaveGame(InSaveGameClass, InSaveIndex))
	{
		if(InSaveIndex == AllSaveGames[SaveName].Array.Num() - 1) AllSaveGames[SaveName].Array.RemoveAt(InSaveIndex);
		else AllSaveGames[SaveName].Array[InSaveIndex] = nullptr;
		if(USaveGameBase* SaveGame = GetSaveGame(InSaveGameClass, InSaveIndex))
		{
			SaveGame->OnUnload();
			SaveGame->ConditionalBeginDestroy();
		}
	}
	return false;
}

bool ASaveGameModule::ResetSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex)
{
	if(!InSaveGameClass) return false;

	const FName SaveName = InSaveGameClass.GetDefaultObject()->GetSaveName();
	if(HasSaveGame(InSaveGameClass, InSaveIndex))
	{
		if(USaveGameBase* SaveGame = GetSaveGame(InSaveGameClass, InSaveIndex))
		{
			SaveGame->OnReset();
		}
	}
	return false;
}

bool ASaveGameModule::RefreshSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex)
{
	if(!InSaveGameClass) return false;

	const FName SaveName = InSaveGameClass.GetDefaultObject()->GetSaveName();
	if(HasSaveGame(InSaveGameClass, InSaveIndex))
	{
		if(USaveGameBase* SaveGame = GetSaveGame(InSaveGameClass, InSaveIndex))
		{
			SaveGame->OnRefresh();
		}
	}
	return false;
}

bool ASaveGameModule::DestroySaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex)
{
	if(!InSaveGameClass) return false;

	const FName SaveName = InSaveGameClass.GetDefaultObject()->GetSaveName();
	if(HasSaveGame(InSaveGameClass, InSaveIndex))
	{
		if(InSaveIndex == AllSaveGames[SaveName].Array.Num() - 1) AllSaveGames[SaveName].Array.RemoveAt(InSaveIndex);
		else AllSaveGames[SaveName].Array[InSaveIndex] = nullptr;
		if(USaveGameBase* SaveGame = GetSaveGame(InSaveGameClass, InSaveIndex))
		{
			SaveGame->OnUnload();
			SaveGame->OnDestroy();
			SaveGame->ConditionalBeginDestroy();
		}
	}
	if (UGameplayStatics::DoesSaveGameExist(GetSaveSlotName(SaveName, InSaveIndex), UserIndex))
	{
		UGameplayStatics::DeleteGameInSlot(GetSaveSlotName(SaveName, InSaveIndex), UserIndex);
		return true;
	}
	return false;
}

void ASaveGameModule::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
}

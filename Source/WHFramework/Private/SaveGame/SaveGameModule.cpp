// Fill out your copyright notice in the Description page of Project Settings.


#include "SaveGame/SaveGameModule.h"

#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "SaveGame/SaveGameModuleBPLibrary.h"
#include "SaveGame/Base/SaveGameBase.h"
#include "SaveGame/General/GeneralSaveGame.h"
		
MODULE_INSTANCE_IMPLEMENTATION(ASaveGameModule)

// ParamSets default values
ASaveGameModule::ASaveGameModule()
{
	ModuleName = FName("SaveGameModule");

	UserIndex = 0;
	
	GeneralSaveGame = UGeneralSaveGame::StaticClass();
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

	LoadSaveData(LoadOrCreateSaveGame(GeneralSaveGame, 0)->GetSaveData());
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

void ASaveGameModule::OnTermination_Implementation()
{
	Super::OnTermination_Implementation();

	SaveSaveGame<UGeneralSaveGame>(0, true);
}

void ASaveGameModule::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	const auto& SaveData = InSaveData->CastRef<FGeneralSaveData>();
	for(auto Iter1 : SaveData.AllSaveGameInfo)
	{
		for(auto Iter2 : Iter1.Value.Array)
		{
			LoadSaveGame(Iter1.Value.Class, Iter2.Key);
		}
		if(AllSaveGameInfo.Contains(Iter1.Key))
		{
			AllSaveGameInfo[Iter1.Key].Class = Iter1.Value.Class;
			AllSaveGameInfo[Iter1.Key].Index = Iter1.Value.Index;
		}
	}
}

FSaveData* ASaveGameModule::ToData()
{
	static FGeneralSaveData SaveData;
	SaveData = FGeneralSaveData();
	for(auto Iter1 : AllSaveGameInfo)
	{
		FSaveGameInfo SaveGameInfo;
		for(auto Iter2 : Iter1.Value.Array)
		{
			if(Iter2.Value && Iter2.Value->IsSaved())
			{
				SaveGameInfo.Array.Add(Iter2.Key, Iter2.Value);
			}
		}
		if(!SaveGameInfo.Array.IsEmpty())
		{
			SaveGameInfo.Class = Iter1.Value.Class;
			SaveGameInfo.Index = SaveGameInfo.Array.Contains(Iter1.Value.Index) ? Iter1.Value.Index : GetSaveGameInfo(Iter1.Value.Class, true).Index;
			SaveData.AllSaveGameInfo.Add(Iter1.Key, SaveGameInfo);
		}
	}
	return &SaveData;
}

FString ASaveGameModule::GetSaveSlotName(FName InSaveName, int32 InSaveIndex) const
{
	return InSaveIndex > 0 ? FString::Printf(TEXT("SaveGame_%s%d"), *InSaveName.ToString(), InSaveIndex) : FString::Printf(TEXT("SaveGame_%s"), *InSaveName.ToString());
}

bool ASaveGameModule::HasSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex, bool bFindOnDisk, bool bNeedLoaded) const
{
	if(!InSaveGameClass) return false;

	if(InSaveIndex == -1) InSaveIndex = GetActiveSaveIndex(InSaveGameClass);

	const FName SaveName = InSaveGameClass.GetDefaultObject()->GetSaveName();
	if(!bFindOnDisk)
	{
		return AllSaveGameInfo.Contains(SaveName) && AllSaveGameInfo[SaveName].Array.Contains(InSaveIndex) && AllSaveGameInfo[SaveName].Array[InSaveIndex] && (!bNeedLoaded || AllSaveGameInfo[SaveName].Array[InSaveIndex]->IsLoaded());
	}
	else
	{
		return UGameplayStatics::DoesSaveGameExist(GetSaveSlotName(SaveName, InSaveIndex), UserIndex);
	}
}

int32 ASaveGameModule::GetValidSaveIndex(TSubclassOf<USaveGameBase> InSaveGameClass) const
{
	if(!InSaveGameClass) return -1;

	const FName SaveName = InSaveGameClass.GetDefaultObject()->GetSaveName();
	int32 SaveIndex = 0;
	if(AllSaveGameInfo.Contains(SaveName))
	{
		for(auto Iter : AllSaveGameInfo[SaveName].Array)
		{
			if(Iter.Key != SaveIndex) break;
			SaveIndex++;
		}
	}
	return SaveIndex;
}

int32 ASaveGameModule::GetActiveSaveIndex(TSubclassOf<USaveGameBase> InSaveGameClass) const
{
	if(!InSaveGameClass) return -1;

	const FName SaveName = InSaveGameClass.GetDefaultObject()->GetSaveName();
	if(AllSaveGameInfo.Contains(SaveName))
	{
		return AllSaveGameInfo[SaveName].Index;
	}
	return -1;
}

void ASaveGameModule::SetActiveSaveIndex(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex)
{
	if(!InSaveGameClass) return;

	const FName SaveName = InSaveGameClass.GetDefaultObject()->GetSaveName();
	if(AllSaveGameInfo.Contains(SaveName))
	{
		if(AllSaveGameInfo[SaveName].Index != InSaveIndex)
		{
			if(HasSaveGame(InSaveGameClass))
			{
				GetSaveGame(InSaveGameClass)->OnActiveChange(false);
			}
			AllSaveGameInfo[SaveName].Index = InSaveIndex;
			if(HasSaveGame(InSaveGameClass))
			{
				GetSaveGame(InSaveGameClass)->OnActiveChange(true);
			}
		}
	}
}

FSaveGameInfo ASaveGameModule::GetSaveGameInfo(TSubclassOf<USaveGameBase> InSaveGameClass, bool bFromGeneralData) const
{
	if(!InSaveGameClass) FSaveGameInfo();

	FSaveGameInfo SaveGameInfo;
	
	const FName SaveName = InSaveGameClass.GetDefaultObject()->GetSaveName();
	if(!bFromGeneralData)
	{
		if(AllSaveGameInfo.Contains(SaveName))
		{
			SaveGameInfo = AllSaveGameInfo[SaveName];
		}
	}
	else
	{
		const auto& GeneralSaveData = GetSaveGame<UGeneralSaveGame>()->GetSaveDataRef<FGeneralSaveData>();
		if(GeneralSaveData.AllSaveGameInfo.Contains(SaveName))
		{
			SaveGameInfo = GeneralSaveData.AllSaveGameInfo[SaveName];
		}
	}
	return SaveGameInfo;
}

USaveGameBase* ASaveGameModule::GetSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex) const
{
	if(!InSaveGameClass) return nullptr;

	if(InSaveIndex == -1) InSaveIndex = GetActiveSaveIndex(InSaveGameClass);

	const FName SaveName = InSaveGameClass.GetDefaultObject()->GetSaveName();
	if(HasSaveGame(InSaveGameClass, InSaveIndex))
	{
		return AllSaveGameInfo[SaveName].Array[InSaveIndex];
	}
	return nullptr;
}

TArray<USaveGameBase*> ASaveGameModule::GetSaveGames(TSubclassOf<USaveGameBase> InSaveGameClass) const
{
	if(!InSaveGameClass) return TArray<USaveGameBase*>();

	TArray<USaveGameBase*> SaveGames;
	const FName SaveName = InSaveGameClass.GetDefaultObject()->GetSaveName();
	if(AllSaveGameInfo.Contains(SaveName))
	{
		AllSaveGameInfo[SaveName].Array.GenerateValueArray(SaveGames);
	}
	return SaveGames;
}

USaveGameBase* ASaveGameModule::CreateSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex, bool bAutoLoad)
{
	if(!InSaveGameClass) return nullptr;
		
	if(InSaveIndex == -1) InSaveIndex = GetValidSaveIndex(InSaveGameClass);

	const FName SaveName = InSaveGameClass.GetDefaultObject()->GetSaveName();
	if(!HasSaveGame(InSaveGameClass, InSaveIndex))
	{
		if(USaveGameBase* SaveGame = Cast<USaveGameBase>(UGameplayStatics::CreateSaveGameObject(InSaveGameClass)))
		{
			if(!AllSaveGameInfo.Contains(SaveName)) AllSaveGameInfo.Add(SaveName);
			AllSaveGameInfo[SaveName].Class = InSaveGameClass;
			AllSaveGameInfo[SaveName].Array.Emplace(InSaveIndex, SaveGame);
			SaveGame->OnCreate(InSaveIndex);
			SetActiveSaveIndex(InSaveGameClass, InSaveIndex);
			if(bAutoLoad)
			{
				SaveGame->Load(EPhase::Primary);
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

USaveGameBase* ASaveGameModule::LoadOrCreateSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex)
{
	return USaveGameModuleBPLibrary::HasSaveGame(InSaveGameClass, InSaveIndex, true) ? USaveGameModuleBPLibrary::LoadSaveGame(InSaveGameClass, InSaveIndex, EPhase::Primary) : USaveGameModuleBPLibrary::CreateSaveGame(InSaveGameClass, InSaveIndex, true);
}

bool ASaveGameModule::SaveSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex, bool bRefresh)
{
	if(!InSaveGameClass) return false;

	if(InSaveIndex == -1) InSaveIndex = GetActiveSaveIndex(InSaveGameClass);

	const FName SaveName = InSaveGameClass.GetDefaultObject()->GetSaveName();
	if(HasSaveGame(InSaveGameClass, InSaveIndex))
	{
		if(USaveGameBase* SaveGame = GetSaveGame(InSaveGameClass, InSaveIndex))
		{
			if(InSaveIndex == GetActiveSaveIndex(InSaveGameClass))
			{
				SaveGame->bSaved = true;
				if(bRefresh)
				{
					SaveGame->OnRefresh();
				}
				SaveGame->GetSaveData()->MakeSaved();
				SaveGame->OnSave();
			}
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
	if(AllSaveGameInfo.Contains(SaveName))
	{
		for(int32 i = 0; i < AllSaveGameInfo[SaveName].Array.Num(); i++)
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
	for(auto Iter1 : AllSaveGameInfo)
	{
		for(auto Iter2 : Iter1.Value.Array)
		{
			if(!Iter2.Value->Save(bRefresh))
			{
				bIsAllSaved = false;
			}
		}
	}
	return bIsAllSaved;
}

USaveGameBase* ASaveGameModule::LoadSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex, EPhase InPhase)
{
	if(!InSaveGameClass) return nullptr;

	if(InSaveIndex == -1) InSaveIndex = GetActiveSaveIndex(InSaveGameClass);

	const FName SaveName = InSaveGameClass.GetDefaultObject()->GetSaveName();
	if(HasSaveGame(InSaveGameClass, InSaveIndex, false))
	{
		if(USaveGameBase* SaveGame = GetSaveGame(InSaveGameClass, InSaveIndex))
		{
			if(InPhase == EPhase::Primary || InSaveIndex == GetActiveSaveIndex(InSaveGameClass))
			{
				SetActiveSaveIndex(InSaveGameClass, InSaveIndex);
				SaveGame->bLoaded = true;
				SaveGame->OnLoad(InPhase);
			}
			return SaveGame;
		}
	}
	else if(HasSaveGame(InSaveGameClass, InSaveIndex, true))
	{
		if(USaveGameBase* SaveGame = Cast<USaveGameBase>(UGameplayStatics::LoadGameFromSlot(GetSaveSlotName(SaveName, InSaveIndex), UserIndex)))
		{
			if(!AllSaveGameInfo.Contains(SaveName)) AllSaveGameInfo.Add(SaveName);
			AllSaveGameInfo[SaveName].Array.Emplace(InSaveIndex, SaveGame);
			SaveGame->GetSaveData()->MakeSaved();
			if(InPhase == EPhase::Primary || InSaveIndex == GetActiveSaveIndex(InSaveGameClass))
			{
				SetActiveSaveIndex(InSaveGameClass, InSaveIndex);
				SaveGame->bLoaded = true;
				SaveGame->OnLoad(InPhase);
			}
			return SaveGame;
		}
	}
	return nullptr;
}

bool ASaveGameModule::UnloadSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex, EPhase InPhase)
{
	if(!InSaveGameClass) return false;

	if(InSaveIndex == -1) InSaveIndex = GetActiveSaveIndex(InSaveGameClass);

	const FName SaveName = InSaveGameClass.GetDefaultObject()->GetSaveName();
	if(HasSaveGame(InSaveGameClass, InSaveIndex))
	{
		if(USaveGameBase* SaveGame = GetSaveGame(InSaveGameClass, InSaveIndex))
		{
			if(InPhase == EPhase::Primary) AllSaveGameInfo[SaveName].Array.Emplace(InSaveIndex, nullptr);
			if(InSaveIndex == GetActiveSaveIndex(InSaveGameClass))
			{
				SaveGame->bLoaded = false;
				SaveGame->OnUnload(InPhase);
				SetActiveSaveIndex(InSaveGameClass, -1);
			}
			SaveGame->ConditionalBeginDestroy();
		}
	}
	return false;
}

bool ASaveGameModule::ResetSaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex)
{
	if(!InSaveGameClass) return false;

	if(InSaveIndex == -1) InSaveIndex = GetActiveSaveIndex(InSaveGameClass);

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

	if(InSaveIndex == -1) InSaveIndex = GetActiveSaveIndex(InSaveGameClass);

	if(HasSaveGame(InSaveGameClass, InSaveIndex))
	{
		if(USaveGameBase* SaveGame = GetSaveGame(InSaveGameClass, InSaveIndex))
		{
			if(InSaveIndex == GetActiveSaveIndex(InSaveGameClass))
			{
				SaveGame->OnRefresh();
			}
		}
	}
	return false;
}

bool ASaveGameModule::DestroySaveGame(TSubclassOf<USaveGameBase> InSaveGameClass, int32 InSaveIndex)
{
	if(!InSaveGameClass) return false;

	if(InSaveIndex == -1) InSaveIndex = GetActiveSaveIndex(InSaveGameClass);

	const FName SaveName = InSaveGameClass.GetDefaultObject()->GetSaveName();
	if(HasSaveGame(InSaveGameClass, InSaveIndex))
	{
		if(USaveGameBase* SaveGame = GetSaveGame(InSaveGameClass, InSaveIndex))
		{
			if(InSaveIndex == GetActiveSaveIndex(InSaveGameClass))
			{
				SaveGame->OnUnload(EPhase::Primary);
				SetActiveSaveIndex(InSaveGameClass, -1);
			}
			SaveGame->OnDestroy();
			SaveGame->ConditionalBeginDestroy();
		}

		AllSaveGameInfo[SaveName].Array.Remove(InSaveIndex);
		if(AllSaveGameInfo[SaveName].Array.Num() == 0) AllSaveGameInfo.Remove(SaveName);

		if(UGameplayStatics::DoesSaveGameExist(GetSaveSlotName(SaveName, InSaveIndex), UserIndex))
		{
			UGameplayStatics::DeleteGameInSlot(GetSaveSlotName(SaveName, InSaveIndex), UserIndex);
		}
		return true;
	}
	return false;
}

void ASaveGameModule::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
}

// Fill out your copyright notice in the Description page of Project Settings.


#include "SaveGame/SaveGameModule.h"

#include "Kismet/GameplayStatics.h"
#include "SaveGame/SaveGameModuleBPLibrary.h"
#include "SaveGame/Base/SaveGameBase.h"
#include "SaveGame/General/GeneralSaveGame.h"
		
IMPLEMENTATION_MODULE(ASaveGameModule)

// ParamSets default values
ASaveGameModule::ASaveGameModule()
{
	ModuleName = FName("SaveGameModule");
	ModuleSaveGame = UGeneralSaveGame::StaticClass();

	UserIndex = 0;
}

ASaveGameModule::~ASaveGameModule()
{
	TERMINATION_MODULE(ASaveGameModule)
}

#if WITH_EDITOR
void ASaveGameModule::OnGenerate()
{
	Super::OnGenerate();
}

void ASaveGameModule::OnDestroy()
{
	Super::OnDestroy();
}
#endif

void ASaveGameModule::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();
}

void ASaveGameModule::OnPreparatory_Implementation(EPhase InPhase)
{
	Super::OnPreparatory_Implementation(InPhase);

	if(PHASEC(InPhase, EPhase::Primary))
	{
		LoadSaveData(LoadOrCreateSaveGame(ModuleSaveGame, 0)->GetSaveData());
	}
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

void ASaveGameModule::OnTermination_Implementation(EPhase InPhase)
{
	Super::OnTermination_Implementation(InPhase);

	if(PHASEC(InPhase, EPhase::Primary))
	{
		SaveSaveGame<UGeneralSaveGame>(0, true, ModuleSaveGame);
	}
	if(PHASEC(InPhase, EPhase::Final))
	{
		for(auto& Iter1 : SaveGameInfos)
		{
			for(const auto Iter2 : Iter1.Value.SaveGames)
			{
				if(Iter2->IsSaved())
				{
					UGameplayStatics::SaveGameToSlot(Iter2, GetSlotName(Iter2->SaveName, Iter2->GetSaveIndex()), UserIndex);
				}
			}
		}
		for(auto Iter : DestroyedSlotNames)
		{
			if(UGameplayStatics::DoesSaveGameExist(Iter, UserIndex))
			{
				UGameplayStatics::DeleteGameInSlot(Iter, UserIndex);
			}
		}
	}
}

void ASaveGameModule::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	const auto& SaveData = InSaveData->CastRef<FGeneralSaveData>();
	for(auto& Iter1 : SaveData.SaveGameDatas)
	{
		const FName SaveName = Iter1.SaveGameClass->GetDefaultObject<USaveGameBase>()->GetSaveName();
		SaveGameInfos.Add(SaveName);
		SaveGameInfos[SaveName].SaveGameClass = Iter1.SaveGameClass;
		SaveGameInfos[SaveName].ActiveIndex = Iter1.ActiveIndex;
		for(const auto Iter2 : Iter1.SaveIndexs)
		{
			LoadSaveGame(Iter1.SaveGameClass, Iter2, EPhase::None);
		}
	}
}

FSaveData* ASaveGameModule::ToData(bool bRefresh)
{
	static FGeneralSaveData SaveData;
	SaveData = FGeneralSaveData();
	
	for(auto& Iter1 : SaveGameInfos)
	{
		FSaveGameData SaveGameData;
		for(const auto Iter2 : Iter1.Value.SaveGames)
		{
			if(Iter2->IsSaved())
			{
				SaveGameData.SaveIndexs.Add(Iter2->SaveIndex);
			}
		}
		if(!SaveGameData.SaveIndexs.IsEmpty())
		{
			SaveGameData.SaveGameClass = Iter1.Value.SaveGameClass;
			SaveGameData.ActiveIndex = SaveGameData.SaveIndexs.Contains(Iter1.Value.ActiveIndex) ? Iter1.Value.ActiveIndex : -1;
			SaveData.SaveGameDatas.Add(SaveGameData);
		}
	}
	return &SaveData;
}

FString ASaveGameModule::GetSlotName(FName InSaveName, int32 InIndex) const
{
	return InIndex != 0 ? FString::Printf(TEXT("SaveGame_%s%d"), *InSaveName.ToString(), InIndex) : FString::Printf(TEXT("SaveGame_%s"), *InSaveName.ToString());
}

bool ASaveGameModule::HasSaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex) const
{
	if(!InClass) return false;

	if(InIndex == -1) InIndex = GetActiveSaveIndex(InClass);

	for(auto Iter : GetSaveGames(InClass))
	{
		if(Iter->SaveIndex == InIndex)
		{
			return true;
		}
	}
	const FName SaveName = GetSaveName(InClass);
	return UGameplayStatics::DoesSaveGameExist(GetSlotName(SaveName, InIndex), UserIndex);
}

FName ASaveGameModule::GetSaveName(TSubclassOf<USaveGameBase> InClass) const
{
	return InClass.GetDefaultObject()->GetSaveName();
}

int32 ASaveGameModule::GetValidSaveIndex(TSubclassOf<USaveGameBase> InClass) const
{
	if(!InClass) return -1;

	int32 SaveIndex = 0;
	for(auto Iter : GetSaveGames(InClass))
	{
		if(Iter->SaveIndex != SaveIndex) break;
		SaveIndex++;
	}
	return SaveIndex;
}

int32 ASaveGameModule::GetActiveSaveIndex(TSubclassOf<USaveGameBase> InClass) const
{
	if(!InClass) return -1;

	return GetSaveGameInfo(InClass).ActiveIndex;
}

FSaveGameInfo ASaveGameModule::GetSaveGameInfo(TSubclassOf<USaveGameBase> InClass) const
{
	if(!InClass) FSaveGameInfo();

	const FName SaveName = GetSaveName(InClass);
	if(SaveGameInfos.Contains(SaveName))
	{
		return SaveGameInfos[SaveName];
	}
	return FSaveGameInfo();
}

USaveGameBase* ASaveGameModule::GetSaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex) const
{
	if(!InClass) return nullptr;

	if(InIndex == -1) InIndex = GetActiveSaveIndex(InClass);

	for(auto Iter : GetSaveGames(InClass))
	{
		if(Iter->SaveIndex == InIndex)
		{
			return Iter;
		}
	}
	return nullptr;
}

TArray<USaveGameBase*> ASaveGameModule::GetSaveGames(TSubclassOf<USaveGameBase> InClass) const
{
	if(!InClass) return TArray<USaveGameBase*>();

	return GetSaveGameInfo(InClass).SaveGames;
}

USaveGameBase* ASaveGameModule::CreateSaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex, EPhase InPhase)
{
	if(!InClass) return nullptr;
		
	if(InIndex == -1) InIndex = GetValidSaveIndex(InClass);

	const FName SaveName = GetSaveName(InClass);
	if(USaveGameBase* SaveGame = Cast<USaveGameBase>(UGameplayStatics::CreateSaveGameObject(InClass)))
	{
		FSaveGameInfo& SaveGameInfo = SaveGameInfos.FindOrAdd(SaveName, FSaveGameInfo(InClass));
		SaveGameInfo.SaveGames.Add(SaveGame);
		const FString SlotName = GetSlotName(SaveName, InIndex);
		if(DestroyedSlotNames.Contains(SlotName))
		{
			DestroyedSlotNames.Remove(SlotName);
		}
		SaveGame->OnCreate(InIndex);
		if(PHASEC(InPhase, EPhase::All))
		{
			SaveGame->Load(InPhase);
		}
		return SaveGame;
	}
	return nullptr;
}

USaveGameBase* ASaveGameModule::GetOrCreateSaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex)
{
	return HasSaveGame(InClass, InIndex) ? GetSaveGame(InClass, InIndex) : CreateSaveGame(InClass, InIndex, EPhase::None);
}

USaveGameBase* ASaveGameModule::LoadOrCreateSaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex, EPhase InPhase)
{
	return USaveGameModuleBPLibrary::HasSaveGame(InClass, InIndex) ? USaveGameModuleBPLibrary::LoadSaveGame(InClass, InIndex, EPhase::Primary) : USaveGameModuleBPLibrary::CreateSaveGame(InClass, InIndex, InPhase);
}

bool ASaveGameModule::SaveSaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex, bool bRefresh)
{
	if(!InClass) return false;

	if(InIndex == -1) InIndex = GetActiveSaveIndex(InClass);

	if(USaveGameBase* SaveGame = GetSaveGame(InClass, InIndex))
	{
		if(SaveGame->IsActived())
		{
			SaveGame->bSaved = true;
			if(bRefresh)
			{
				SaveGame->OnRefresh();
			}
			SaveGame->GetSaveData()->MakeSaved();
			SaveGame->OnSave();
		}
		return true;
	}
	return false;
}

bool ASaveGameModule::SaveSaveGames(TSubclassOf<USaveGameBase> InClass, bool bRefresh)
{
	if(!InClass) return false;
	
	bool bIsAllSaved = true;
	FSaveGameInfo SaveGameInfo = GetSaveGameInfo(InClass);
	for(auto Iter : SaveGameInfo.SaveGames)
	{
		if(Iter && !Iter->Save(bRefresh))
		{
			bIsAllSaved = false;
		}
	}
	return bIsAllSaved;
}

bool ASaveGameModule::SaveAllSaveGame(bool bRefresh)
{
	bool bIsAllSaved = true;
	for(auto Iter1 : SaveGameInfos)
	{
		for(auto Iter2 : Iter1.Value.SaveGames)
		{
			if(Iter2 && !Iter2->Save(bRefresh))
			{
				bIsAllSaved = false;
			}
		}
	}
	return bIsAllSaved;
}

USaveGameBase* ASaveGameModule::LoadSaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex, EPhase InPhase)
{
	if(!InClass) return nullptr;

	if(InIndex == -1) InIndex = GetActiveSaveIndex(InClass);

	const FName SaveName = GetSaveName(InClass);
	USaveGameBase* SaveGame = GetSaveGame(InClass, InIndex);
	if(!SaveGame)
	{
		SaveGame = Cast<USaveGameBase>(UGameplayStatics::LoadGameFromSlot(GetSlotName(SaveName, InIndex), UserIndex));
		SaveGame->GetSaveData()->MakeSaved();
		FSaveGameInfo& SaveGameInfo = SaveGameInfos.FindOrAdd(SaveName, FSaveGameInfo(InClass));
		SaveGameInfo.SaveGames.Add(SaveGame);
	}
	if(SaveGame)
	{
		if(PHASEC(InPhase, EPhase::Primary))
		{
			UnloadSaveGame(InClass, -1, InPhase);
		}
		SaveGame->bLoaded = true;
		if(PHASEC(InPhase, EPhase::All))
		{
			SaveGameInfos[SaveName].ActiveIndex = InIndex;
			SaveGame->OnLoad(InPhase);
		}
		return SaveGame;
	}
	return nullptr;
}

bool ASaveGameModule::UnloadSaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex, EPhase InPhase)
{
	if(!InClass) return false;

	if(InIndex == -1) InIndex = GetActiveSaveIndex(InClass);

	const FName SaveName = GetSaveName(InClass);
	if(USaveGameBase* SaveGame = GetSaveGame(InClass, InIndex))
	{
		if(PHASEC(InPhase, EPhase::Primary))
		{
			if(SaveGame->IsActived())
			{
				SaveGameInfos[SaveName].ActiveIndex = -1;
			}
		}
		SaveGame->OnUnload(InPhase);

		if(PHASEC(InPhase, EPhase::Final))
		{
			SaveGame->bLoaded = false;
			SaveGameInfos[SaveName].SaveGames.Remove(SaveGame);
			if(SaveGameInfos[SaveName].SaveGames.IsEmpty()) SaveGameInfos.Remove(SaveName);
		}
		return true;
	}
	return false;
}

bool ASaveGameModule::ResetSaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex)
{
	if(!InClass) return false;

	if(InIndex == -1) InIndex = GetActiveSaveIndex(InClass);

	if(USaveGameBase* SaveGame = GetSaveGame(InClass, InIndex))
	{
		SaveGame->OnReset();
	}
	return false;
}

bool ASaveGameModule::RefreshSaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex)
{
	if(!InClass) return false;

	if(InIndex == -1) InIndex = GetActiveSaveIndex(InClass);

	if(USaveGameBase* SaveGame = GetSaveGame(InClass, InIndex))
	{
		SaveGame->OnRefresh();
	}
	return false;
}

bool ASaveGameModule::DestroySaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex)
{
	if(!InClass) return false;

	if(InIndex == -1) InIndex = GetActiveSaveIndex(InClass);

	const FName SaveName = GetSaveName(InClass);
	if(USaveGameBase* SaveGame = GetSaveGame(InClass, InIndex))
	{
		if(SaveGame->IsActived())
		{
			SaveGameInfos[SaveName].ActiveIndex = -1;
			SaveGame->OnUnload(EPhase::Primary);
		}
		else
		{
			SaveGame->OnUnload(EPhase::Final);
		}
		SaveGame->OnDestroy();

		DestroyedSlotNames.Add(GetSlotName(SaveName, InIndex));
		
		SaveGameInfos[SaveName].SaveGames.Remove(SaveGame);
		if(SaveGameInfos[SaveName].SaveGames.IsEmpty()) SaveGameInfos.Remove(SaveName);
		return true;
	}
	return false;
}

void ASaveGameModule::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
}

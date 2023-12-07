// Fill out your copyright notice in the Description page of Project Settings.


#include "SaveGame/SaveGameModule.h"

#include "Event/EventModuleStatics.h"
#include "Event/Handle/Common/EventHandle_ExitGame.h"
#include "Kismet/GameplayStatics.h"
#include "SaveGame/SaveGameModuleStatics.h"
#include "SaveGame/Base/SaveGameBase.h"
#include "SaveGame/General/GeneralSaveGame.h"
		
IMPLEMENTATION_MODULE(USaveGameModule)

// ParamSets default values
USaveGameModule::USaveGameModule()
{
	ModuleName = FName("SaveGameModule");
	ModuleDisplayName = FText::FromString(TEXT("Save Game Module"));
	bModuleAutoSave = true;
	ModuleSavePhase = EPhase::Primary;
	ModuleSaveGame = UGeneralSaveGame::StaticClass();

	UserIndex = 0;
}

USaveGameModule::~USaveGameModule()
{
	TERMINATION_MODULE(USaveGameModule)
}

#if WITH_EDITOR
void USaveGameModule::OnGenerate()
{
	Super::OnGenerate();
}

void USaveGameModule::OnDestroy()
{
	Super::OnDestroy();

	TERMINATION_MODULE(USaveGameModule)
}
#endif

void USaveGameModule::OnInitialize()
{
	Super::OnInitialize();
	
	UEventModuleStatics::SubscribeEvent<UEventHandle_ExitGame>(this, FName("OnExitGame"));
}

void USaveGameModule::OnPreparatory(EPhase InPhase)
{
	Super::OnPreparatory(InPhase);
}

void USaveGameModule::OnRefresh(float DeltaSeconds)
{
	Super::OnRefresh(DeltaSeconds);
}

void USaveGameModule::OnPause()
{
	Super::OnPause();
}

void USaveGameModule::OnUnPause()
{
	Super::OnUnPause();
}

void USaveGameModule::OnTermination(EPhase InPhase)
{
	Super::OnTermination(InPhase);
}

void USaveGameModule::OnExitGame(UObject* InSender, UEventHandle_ExitGame* InEventHandle)
{
	if(bModuleAutoSave)
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

void USaveGameModule::LoadData(FSaveData* InSaveData, EPhase InPhase)
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

FSaveData* USaveGameModule::ToData()
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

FString USaveGameModule::GetSlotName(FName InSaveName, int32 InIndex) const
{
	return InIndex != 0 ? FString::Printf(TEXT("SaveGame_%s%d"), *InSaveName.ToString(), InIndex) : FString::Printf(TEXT("SaveGame_%s"), *InSaveName.ToString());
}

bool USaveGameModule::HasSaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex) const
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

FName USaveGameModule::GetSaveName(TSubclassOf<USaveGameBase> InClass) const
{
	return InClass.GetDefaultObject()->GetSaveName();
}

int32 USaveGameModule::GetValidSaveIndex(TSubclassOf<USaveGameBase> InClass) const
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

int32 USaveGameModule::GetActiveSaveIndex(TSubclassOf<USaveGameBase> InClass) const
{
	if(!InClass) return -1;

	return GetSaveGameInfo(InClass).ActiveIndex;
}

FSaveGameInfo USaveGameModule::GetSaveGameInfo(TSubclassOf<USaveGameBase> InClass) const
{
	if(!InClass) FSaveGameInfo();

	const FName SaveName = GetSaveName(InClass);
	if(SaveGameInfos.Contains(SaveName))
	{
		return SaveGameInfos[SaveName];
	}
	return FSaveGameInfo();
}

USaveGameBase* USaveGameModule::GetSaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex) const
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

TArray<USaveGameBase*> USaveGameModule::GetSaveGames(TSubclassOf<USaveGameBase> InClass) const
{
	if(!InClass) return TArray<USaveGameBase*>();

	return GetSaveGameInfo(InClass).SaveGames;
}

USaveGameBase* USaveGameModule::CreateSaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex, EPhase InPhase)
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

USaveGameBase* USaveGameModule::GetOrCreateSaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex, EPhase InPhase)
{
	return HasSaveGame(InClass, InIndex) ? GetSaveGame(InClass, InIndex) : CreateSaveGame(InClass, InIndex, InPhase);
}

USaveGameBase* USaveGameModule::LoadOrCreateSaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex, EPhase InPhase)
{
	return USaveGameModuleStatics::HasSaveGame(InClass, InIndex) ? USaveGameModuleStatics::LoadSaveGame(InClass, InIndex, EPhase::Primary) : USaveGameModuleStatics::CreateSaveGame(InClass, InIndex, InPhase);
}

bool USaveGameModule::SaveSaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex, bool bRefresh)
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

bool USaveGameModule::SaveSaveGames(TSubclassOf<USaveGameBase> InClass, bool bRefresh)
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

bool USaveGameModule::SaveAllSaveGame(bool bRefresh)
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

USaveGameBase* USaveGameModule::LoadSaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex, EPhase InPhase)
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

bool USaveGameModule::UnloadSaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex, EPhase InPhase)
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

bool USaveGameModule::ResetSaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex)
{
	if(!InClass) return false;

	if(InIndex == -1) InIndex = GetActiveSaveIndex(InClass);

	if(USaveGameBase* SaveGame = GetSaveGame(InClass, InIndex))
	{
		SaveGame->OnReset();
	}
	return false;
}

bool USaveGameModule::RefreshSaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex)
{
	if(!InClass) return false;

	if(InIndex == -1) InIndex = GetActiveSaveIndex(InClass);

	if(USaveGameBase* SaveGame = GetSaveGame(InClass, InIndex))
	{
		SaveGame->OnRefresh();
	}
	return false;
}

bool USaveGameModule::DestroySaveGame(TSubclassOf<USaveGameBase> InClass, int32 InIndex)
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

void USaveGameModule::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
}

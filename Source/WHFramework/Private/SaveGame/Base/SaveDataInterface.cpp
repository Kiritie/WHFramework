#pragma once

#include "SaveGame/Base/SaveDataInterface.h"

#include "Global/GlobalBPLibrary.h"

void ISaveDataInterface::LoadSaveData(FSaveData* InSaveData, EPhase InPhase)
{
	if (HasArchive())
	{
		UGlobalBPLibrary::LoadObjectDataFromMemory(Cast<UObject>(this), InSaveData->GetDatas());
	}
	LoadData(InSaveData, InPhase);
}

FSaveData* ISaveDataInterface::GetSaveData(bool bRefresh)
{
	FSaveData* SaveData = ToData(bRefresh);
	if (HasArchive())
	{
		UGlobalBPLibrary::SaveObjectDataToMemory(Cast<UObject>(this), SaveData->GetDatas());
	}
	return SaveData;
}

void ISaveDataInterface::UnloadSaveData(EPhase InPhase)
{
	UnloadData(InPhase);
	if (HasArchive())
	{
		UGlobalBPLibrary::LoadObjectDataFromMemory(Cast<UObject>(this), TArray<uint8>());
	}
}

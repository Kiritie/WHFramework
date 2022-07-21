#pragma once

#include "SaveGame/Base/SaveDataInterface.h"

#include "Global/GlobalBPLibrary.h"

void ISaveDataInterface::LoadSaveData(FSaveData* InSaveData, bool bForceMode, bool bLoadMemoryData)
{
	LoadData(InSaveData, bForceMode);
	if (bLoadMemoryData)
	{
		UGlobalBPLibrary::LoadObjectDataFromMemory(Cast<UObject>(this), InSaveData->GetDatas());
	}
}

FSaveData* ISaveDataInterface::ToSaveData(bool bSaveMemoryData)
{
	FSaveData* SaveData = ToData();
	if (bSaveMemoryData)
	{
		UGlobalBPLibrary::SaveObjectDataToMemory(Cast<UObject>(this), SaveData->GetDatas());
	}
	return SaveData;
}

void ISaveDataInterface::UnloadSaveData(bool bForceMode, bool bUnLoadMemoryData)
{
	UnloadData(bForceMode);
	if (bUnLoadMemoryData)
	{
		UGlobalBPLibrary::LoadObjectDataFromMemory(Cast<UObject>(this), TArray<uint8>());
	}
}

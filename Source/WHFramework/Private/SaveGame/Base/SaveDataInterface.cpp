#pragma once

#include "SaveGame/Base/SaveDataInterface.h"

#include "Global/GlobalBPLibrary.h"

void ISaveDataInterface::LoadSaveData(FSaveData* InSaveData, bool bForceMode, bool bLoadMemoryData)
{
	LoadData(InSaveData, bForceMode);
	if (bLoadMemoryData && HasArchive())
	{
		UGlobalBPLibrary::LoadObjectDataFromMemory(Cast<UObject>(this), InSaveData->GetDatas());
	}
}

FSaveData* ISaveDataInterface::ToSaveData(bool bSaveMemoryData)
{
	FSaveData* SaveData = ToData();
	if (bSaveMemoryData && HasArchive())
	{
		UGlobalBPLibrary::SaveObjectDataToMemory(Cast<UObject>(this), SaveData->GetDatas());
	}
	return SaveData;
}

void ISaveDataInterface::UnloadSaveData(bool bForceMode, bool bUnLoadMemoryData)
{
	UnloadData(bForceMode);
	if (bUnLoadMemoryData && HasArchive())
	{
		UGlobalBPLibrary::LoadObjectDataFromMemory(Cast<UObject>(this), TArray<uint8>());
	}
}

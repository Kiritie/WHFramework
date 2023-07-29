#pragma once

#include "SaveGame/Base/SaveDataInterface.h"

#include "Global/GlobalBPLibrary.h"

void ISaveDataInterface::LoadSaveData(FSaveData* InSaveData, EPhase InPhase)
{
	LocalSaveData = InSaveData;
	LoadData(InSaveData, InPhase);
	if (HasArchive())
	{
		UGlobalBPLibrary::LoadObjectDataFromMemory(Cast<UObject>(this), InSaveData->GetDatas());
	}
}

FSaveData* ISaveDataInterface::GetSaveData(bool bRefresh)
{
	if (bRefresh)
	{
		LocalSaveData = ToData();
	}
	if (HasArchive())
	{
		UGlobalBPLibrary::SaveObjectDataToMemory(Cast<UObject>(this), LocalSaveData->GetDatas());
	}
	return LocalSaveData;
}

void ISaveDataInterface::UnloadSaveData(EPhase InPhase)
{
	UnloadData(InPhase);
	if (HasArchive())
	{
		UGlobalBPLibrary::LoadObjectDataFromMemory(Cast<UObject>(this), TArray<uint8>());
	}
	LocalSaveData = nullptr;
}

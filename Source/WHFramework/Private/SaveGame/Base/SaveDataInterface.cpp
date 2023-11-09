#pragma once

#include "SaveGame/Base/SaveDataInterface.h"

#include "Common/CommonStatics.h"

void ISaveDataInterface::LoadSaveData(FSaveData* InSaveData, EPhase InPhase)
{
	if (PHASEC(InPhase, EPhase::Final) && HasArchive())
	{
		UCommonStatics::LoadObjectDataFromMemory(Cast<UObject>(this), InSaveData->GetDatas());
	}
	LoadData(InSaveData, InPhase);
}

FSaveData* ISaveDataInterface::GetSaveData(bool bRefresh)
{
	FSaveData* SaveData = !bRefresh ? GetData() : ToData();
	if (HasArchive())
	{
		UCommonStatics::SaveObjectDataToMemory(Cast<UObject>(this), SaveData->GetDatas());
	}
	return SaveData;
}

void ISaveDataInterface::UnloadSaveData(EPhase InPhase)
{
	UnloadData(InPhase);
	if (PHASEC(InPhase, EPhase::Final) && HasArchive())
	{
		UCommonStatics::LoadObjectDataFromMemory(Cast<UObject>(this), TArray<uint8>());
	}
}

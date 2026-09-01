#pragma once

#include "SaveGame/Base/SaveDataAgentInterface.h"

#include "Common/CommonModuleStatics.h"

void ISaveDataAgentInterface::LoadSaveData(FSaveData* InSaveData, EPhase InPhase)
{
	if (PHASEC(InPhase, EPhase::Final) && HasArchive())
	{
		UCommonModuleStatics::LoadObjectDataFromMemory(Cast<UObject>(this), InSaveData->GetDatas());
	}
	LoadData(InSaveData, InPhase);
}

FSaveData* ISaveDataAgentInterface::GetSaveData(bool bRefresh)
{
	FSaveData* SaveData = !bRefresh ? GetData() : ToData();
	if (HasArchive())
	{
		UCommonModuleStatics::SaveObjectDataToMemory(Cast<UObject>(this), SaveData->GetDatas());
	}
	return SaveData;
}

void ISaveDataAgentInterface::UnloadSaveData(EPhase InPhase)
{
	UnloadData(InPhase);
	if (PHASEC(InPhase, EPhase::Final) && HasArchive())
	{
		UCommonModuleStatics::LoadObjectDataFromMemory(Cast<UObject>(this), TArray<uint8>());
	}
}

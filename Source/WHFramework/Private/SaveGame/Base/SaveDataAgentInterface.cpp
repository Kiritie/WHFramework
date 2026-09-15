#include "SaveGame/Base/SaveDataAgentInterface.h"

#include "Common/CommonModuleStatics.h"

const FSaveData* ISaveDataAgentInterface::ResolveSaveData(const FParameter& InParameter)
{
	const UScriptStruct* Struct = InParameter.GetStructType();
	const uint8* Memory = InParameter.GetStructMemory();
	return Struct && Memory && Struct->IsChildOf(FSaveData::StaticStruct()) ? reinterpret_cast<const FSaveData*>(Memory) : nullptr;
}

FSaveData* ISaveDataAgentInterface::ResolveSaveData(FParameter& InParameter)

{
	const UScriptStruct* Struct = InParameter.GetStructType();
	uint8* Memory = InParameter.GetMutableStructMemory();
	return Struct && Memory && Struct->IsChildOf(FSaveData::StaticStruct()) ? reinterpret_cast<FSaveData*>(Memory) : nullptr;
}

void ISaveDataAgentInterface::LoadSaveData(const FParameter& InSaveData, EPhase InPhase)
{
	if(!InSaveData.HasValue())
	{
		return;
	}

	FParameter SaveData = InSaveData;
	if(FSaveData* Data = ResolveSaveData(SaveData))
	{
		Data->MakeSaved();
	}
	if(PHASEC(InPhase, EPhase::Final) && HasArchive())
	{
		if(const FSaveData* Data = ResolveSaveData(SaveData))
		{
			UCommonModuleStatics::LoadObjectDataFromMemory(Cast<UObject>(this), Data->GetDatas());
		}
	}
	LoadData(SaveData, InPhase);
}

FParameter ISaveDataAgentInterface::GetSaveData(bool bRefresh)
{
	FParameter SaveData = bRefresh ? ToData() : GetData();
	if(!SaveData.HasValue())
	{
		SaveData = ToData();
	}
	if(!SaveData.HasValue())
	{
		return FParameter();
	}
	if(HasArchive())
	{
		if(FSaveData* Data = ResolveSaveData(SaveData))
		{
			UCommonModuleStatics::SaveObjectDataToMemory(Cast<UObject>(this), Data->GetDatas());
		}
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
